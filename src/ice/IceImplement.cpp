// =============================================================================
// IceImplement.cpp — 描く準備と draw3D
//
// 形づくり (generateShape など) は IceGenerate.cpp にある。
// =============================================================================

#include "IceImplement.h"

#include "engine/DxCheck.h"
#include "engine/ShaderCompiler.h"

#include <cstdio>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace {

// IceCommon.hlsli の cbuffer IceFrame (b0) と同じ並び。 フレームごとに変わる値 (32 bit × 20 個)
struct IceFrameConstants {
    Float4x4 viewProj;  // ワールド → クリップ空間 (転置済み)
    Float3 cameraPos;   // カメラの位置
    float time;         // 起動からの秒
};
static_assert(sizeof(IceFrameConstants) == 20 * 4, "IceFrameConstants must be twenty 32-bit values");

// IceCommon.hlsli の cbuffer IceObject (b1) と同じ並び。 氷ごとの値 (32 bit × 4 個)
struct IceObjectConstants {
    Float3 position;  // 氷の位置 (ワールド)
    float pad;        // 16 バイトにそろえる
};
static_assert(sizeof(IceObjectConstants) == 4 * 4, "IceObjectConstants must be four 32-bit values");

// ルート引数の番号
constexpr UINT kRootFrame = 0;   // b0: IceFrameConstants
constexpr UINT kRootObject = 1;  // b1: IceObjectConstants

} // namespace

void IceImplement::initialize(ID3D12Device* device, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat,
                              const std::wstring& shaderDir)
{
    mDevice = device;
    mColorFormat = colorFormat;
    mDepthFormat = depthFormat;
    mShaderDir = shaderDir;

    createRootSignature();
    std::string error;
    if (!buildPipelines(error)) throw std::runtime_error("Ice shader error:\n" + error);
}

void IceImplement::createRootSignature()
{
    // ルート引数 (シェーダーへ渡す物の一覧) は 2 つ。 どちらも 32 bit 定数を直接置く (定数バッファを作らない)
    //   0: b0 IceFrame  — カメラなど、 フレームごとの値
    //   1: b1 IceObject — 位置など、 氷ごとの値
    D3D12_ROOT_PARAMETER params[2] = {};
    params[kRootFrame].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[kRootFrame].Constants.ShaderRegister = 0;
    params[kRootFrame].Constants.Num32BitValues = sizeof(IceFrameConstants) / 4;
    params[kRootFrame].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;  // 頂点とピクセルの両方で使う

    params[kRootObject].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[kRootObject].Constants.ShaderRegister = 1;
    params[kRootObject].Constants.Num32BitValues = sizeof(IceObjectConstants) / 4;
    params[kRootObject].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 2;
    desc.pParameters = params;
    // 頂点バッファ (入力レイアウト) を使うので、 このフラグが要る
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob, error;
    ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error),
                  "D3D12SerializeRootSignature(ice)");
    ThrowIfFailed(mDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                               IID_PPV_ARGS(&mRootSignature)),
                  "CreateRootSignature(ice)");
}

bool IceImplement::buildPipelines(std::string& error)
{
    // 1. シェーダーをコンパイルする
    const std::wstring path = mShaderDir + L"/IceMesh.hlsl";
    std::string log;
    ComPtr<ID3DBlob> vs = ShaderCompiler::compile(path, "VSMain", "vs_5_0", log);
    ComPtr<ID3DBlob> ps;
    if (vs) ps = ShaderCompiler::compile(path, "PSMain", "ps_5_0", log);
    if (!vs || !ps) {
        error = log;
        return false;
    }

    // 2. 頂点の並び (IceVertex と同じ)。 セマンティクスの名前で HLSL の VSInput とつながる
    const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = mRootSignature.Get();
    desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    desc.InputLayout = { inputLayout, static_cast<UINT>(sizeof(inputLayout) / sizeof(inputLayout[0])) };

    // ブレンドしない (上書き)。 BlendEnable は FALSE だが、 列挙の値は 0 だと不正なので正しい値を入れる
    D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.SampleMask = UINT_MAX;

    // 裏面を描かない。 形の三角形は 「外から見て反時計回り」 に並べてあるので、 反時計回りを表にする
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    desc.RasterizerState.FrontCounterClockwise = TRUE;
    desc.RasterizerState.DepthClipEnable = TRUE;

    // 深度テスト: 手前の面だけを残す (深度が小さいほど手前)。 深度にも書く
    const D3D12_DEPTH_STENCILOP_DESC keep = {
        D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    desc.DepthStencilState.DepthEnable = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    desc.DepthStencilState.FrontFace = keep;
    desc.DepthStencilState.BackFace = keep;

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = mColorFormat;
    desc.DSVFormat = mDepthFormat;
    desc.SampleDesc.Count = 1;

    // 3. 作る。 成功したときだけ差し替えるので、 読み直しに失敗しても前の PSO で描き続けられる
    ComPtr<ID3D12PipelineState> pipeline;
    const HRESULT hr = mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline));
    if (FAILED(hr)) {
        char text[96];
        std::snprintf(text, sizeof(text), "CreateGraphicsPipelineState(ice mesh) failed (HRESULT 0x%08X)",
                      static_cast<unsigned>(hr));
        error = text;
        return false;
    }
    mMeshPipeline = pipeline;
    return true;
}

bool IceImplement::reloadShaders(std::string& error)
{
    return buildPipelines(error);
}

void IceImplement::draw3D(const DrawContext& context)
{
    ID3D12GraphicsCommandList* commandList = context.commandList;

    // 1. 最初の 1 回だけ、 全部の形を作って mGeom に入れる (IceGenerate.cpp)
    if (!mGenerated) {
        generateShape();
        mGenerated = true;
    }

    // 2. 今の形を選ぶ。 形の種類がそのまま mGeom の添え字になる
    const IceGeometry& geom = mGeom[static_cast<int>(mParams.shape)];

    // 3. シェーダーへ渡す値
    const IceFrameConstants frame = { context.viewProj, context.cameraPos, context.time };
    const IceObjectConstants object = { mParams.position, 0.0f };

    // 4. 氷のパイプラインに切り替えて描く。
    //    描画先・ビューポート・シザーはエンジン (Renderer::beginFrame) が設定済み
    commandList->SetGraphicsRootSignature(mRootSignature.Get());
    commandList->SetPipelineState(mMeshPipeline.Get());
    commandList->SetGraphicsRoot32BitConstants(kRootFrame, sizeof(frame) / 4, &frame, 0);
    commandList->SetGraphicsRoot32BitConstants(kRootObject, sizeof(object) / 4, &object, 0);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &geom.vertexView);
    commandList->IASetIndexBuffer(&geom.indexView);
    commandList->DrawIndexedInstanced(geom.indexCount, 1, 0, 0, 0);
}
