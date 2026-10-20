#include "DisplayPass.h"

#include "DxCheck.h"
#include "ShaderCompiler.h"

#include <cstdio>
#include <utility>

using Microsoft::WRL::ComPtr;

DisplayPass::DisplayPass(ID3D12Device* device, DXGI_FORMAT outputFormat, std::wstring shaderPath)
    : mDevice(device), mOutputFormat(outputFormat), mShaderPath(std::move(shaderPath))
{
    createRootSignature();
}

void DisplayPass::createRootSignature()
{
    // ルート引数は 1 つだけ: シーンの描画先の SRV (t0) を指すデスクリプターテーブル。
    // 同じ大きさの画素を 1 対 1 で読む (Load) ので、 サンプラーは要らない
    D3D12_DESCRIPTOR_RANGE range = {};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;  // t0

    D3D12_ROOT_PARAMETER param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    param.DescriptorTable.pDescriptorRanges = &range;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 1;
    desc.pParameters = &param;

    ComPtr<ID3DBlob> blob, error;
    ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error),
                  "D3D12SerializeRootSignature(display)");
    ThrowIfFailed(mDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                               IID_PPV_ARGS(&mRootSignature)),
                  "CreateRootSignature(display)");
}

bool DisplayPass::build(std::string& error)
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

    // ブレンドしない (上書き)
    D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.SampleMask = UINT_MAX;

    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;  // 全画面の三角形 1 枚
    desc.RasterizerState.DepthClipEnable = TRUE;

    // 深度は使わない (バックバッファだけに描く)
    const D3D12_DEPTH_STENCILOP_DESC keep = {
        D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.DepthStencilState.FrontFace = keep;
    desc.DepthStencilState.BackFace = keep;

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = mOutputFormat;
    desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;

    ComPtr<ID3D12PipelineState> pipelineState;
    const HRESULT hr = mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) {
        char text[96];
        std::snprintf(text, sizeof(text), "CreateGraphicsPipelineState(display) failed (HRESULT 0x%08X)",
                      static_cast<unsigned>(hr));
        error = text;
        return false;
    }
    mPipelineState = pipelineState;
    return true;
}

void DisplayPass::draw(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* srvHeap,
                       D3D12_GPU_DESCRIPTOR_HANDLE sceneColor)
{
    commandList->SetPipelineState(mPipelineState.Get());
    commandList->SetGraphicsRootSignature(mRootSignature.Get());
    ID3D12DescriptorHeap* heaps[] = { srvHeap };
    commandList->SetDescriptorHeaps(1, heaps);
    commandList->SetGraphicsRootDescriptorTable(0, sceneColor);
    // 頂点バッファは使わない。 頂点シェーダーが頂点の番号から全画面の三角形を作る
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
}
