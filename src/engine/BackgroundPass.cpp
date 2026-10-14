#include "BackgroundPass.h"

#include "DxCheck.h"
#include "ShaderCompiler.h"

#include <cstdio>
#include <utility>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace {

// Background.hlsl の cbuffer BackgroundConstants と同じ並び (32 bit × 20 個)
struct BackgroundConstants {
    XMFLOAT4X4 invViewProj;
    XMFLOAT3 cameraPos;
    float time;
};
static_assert(sizeof(BackgroundConstants) == 20 * 4, "BackgroundConstants must be twenty 32-bit values");

} // namespace

BackgroundPass::BackgroundPass(ID3D12Device* device, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat,
                               std::wstring shaderPath)
    : mDevice(device), mColorFormat(colorFormat), mDepthFormat(depthFormat), mShaderPath(std::move(shaderPath))
{
    createRootSignature();
}

void BackgroundPass::createRootSignature()
{
    // ルート引数は 1 つだけ: 32 bit 定数 20 個を b0 に直接置く (定数バッファのリソースは作らない)
    D3D12_ROOT_PARAMETER param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param.Constants.ShaderRegister = 0;
    param.Constants.Num32BitValues = sizeof(BackgroundConstants) / 4;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 1;
    desc.pParameters = &param;

    // ルートシグネチャは、 いったんバイト列に直して (Serialize) から作る
    ComPtr<ID3DBlob> blob, error;
    ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error),
                  "D3D12SerializeRootSignature(background)");
    ThrowIfFailed(mDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                               IID_PPV_ARGS(&mRootSignature)),
                  "CreateRootSignature(background)");
}

bool BackgroundPass::build(std::string& error)
{
    std::string log;
    ComPtr<ID3DBlob> vs = ShaderCompiler::compile(mShaderPath, "VSMain", "vs_5_0", log);
    ComPtr<ID3DBlob> ps;
    if (vs) ps = ShaderCompiler::compile(mShaderPath, "PSMain", "ps_5_0", log);
    if (!vs || !ps) {
        error = log;
        return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = mRootSignature.Get();
    desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };

    // ブレンドしない (上書き)。 BlendEnable は FALSE だが、 列挙の値は 0 だと不正なので正しい値を入れる
    D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.SampleMask = UINT_MAX;

    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;  // 全画面の三角形 1 枚なので裏表は気にしない
    desc.RasterizerState.DepthClipEnable = TRUE;

    // 深度は読みも書きもしない。 列挙の値だけ正しい値にしておく
    const D3D12_DEPTH_STENCILOP_DESC keep = {
        D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.DepthStencilState.FrontFace = keep;
    desc.DepthStencilState.BackFace = keep;

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = mColorFormat;
    desc.DSVFormat = mDepthFormat;  // 深度バッファが付いた描画先で使うので、 形式だけ合わせる
    desc.SampleDesc.Count = 1;

    // 成功したときだけ差し替える。 失敗しても前の PSO で描き続けられる
    ComPtr<ID3D12PipelineState> pipelineState;
    const HRESULT hr = mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) {
        char text[96];
        std::snprintf(text, sizeof(text), "CreateGraphicsPipelineState(background) failed (HRESULT 0x%08X)",
                      static_cast<unsigned>(hr));
        error = text;
        return false;
    }
    mPipelineState = pipelineState;
    return true;
}

void BackgroundPass::draw(ID3D12GraphicsCommandList* commandList, const XMFLOAT4X4& invViewProj,
                          const XMFLOAT3& cameraPos, float time)
{
    const BackgroundConstants constants = { invViewProj, cameraPos, time };

    commandList->SetPipelineState(mPipelineState.Get());
    commandList->SetGraphicsRootSignature(mRootSignature.Get());
    commandList->SetGraphicsRoot32BitConstants(0, sizeof(constants) / 4, &constants, 0);
    // 頂点バッファは使わない。 頂点シェーダーが頂点の番号 (0, 1, 2) から全画面の三角形を作る
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
}
