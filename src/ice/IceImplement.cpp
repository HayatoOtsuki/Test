// =============================================================================
// IceImplement.cpp — 描く準備と draw3D
//
// 形づくり (generateShape など) は IceGenerate.cpp にある。
// =============================================================================

#include "IceImplement.h"

#include "engine/DxCheck.h"
#include "engine/ShaderCompiler.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace {

// IceCommon.hlsli の cbuffer IceObject (b1) と同じ並び。 氷ごとの値 (32 bit × 24 個)。
// HLSL の定数は 16 バイト (float 4 個) ずつに区切られるので、 float3 の後には float を 1 つ置く。
// Material は float4 + float 3 個 = 28 バイトなので、 float を 1 つ足して 32 バイトにそろえる
struct IceObjectConstants {
    Material material;  // [0] baseColor  [1] metallic roughness specular
    float pad0;         // [1] w
    Float3 position;    // [2] 氷の位置 (ワールド)
    float refractiveIndex;  // [2] w 屈折率
    Float3 boundsMin;   // [3] SDF の格子が覆う箱の最小の角 (物体の空間)
    float voxelSize;    // [3] w 格子 1 つ分の大きさ
    Float3 boundsMax;   // [4] 同じく最大の角
    float pad2;         // [4] w
    Float3 absorption;  // [5] 吸収の係数 σa (1/m)。 吸収の色から CPU で計算する
    float pad3;         // [5] w
};
static_assert(sizeof(IceObjectConstants) == 24 * 4, "IceObjectConstants must be twenty-four 32-bit values");

// ルート引数の番号
constexpr UINT kRootScene = 0;   // b0: エンジンのシーンの定数 (カメラとライト)
constexpr UINT kRootObject = 1;  // b1: IceObjectConstants
constexpr UINT kRootSdf = 2;     // t0: SDF の 3D テクスチャ (デスクリプターテーブル)

} // namespace

IceImplement::IceImplement(std::wstring shaderDir) : mShaderDir(std::move(shaderDir))
{
}

void IceImplement::initialize(const RenderSetup& setup)
{
    mDevice = setup.device;
    mColorFormat = setup.colorFormat;
    mDepthFormat = setup.depthFormat;

    // SDF の 3D テクスチャの SRV を置くヒープ。 シェーダーから読むので SHADER_VISIBLE にする。
    // 中身 (SRV) は generateSDF で作る
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mSrvHeap)), "CreateDescriptorHeap(ice SRV)");

    createRootSignature();
    std::string error;
    if (!buildPipelines(error)) throw std::runtime_error("Ice shader error:\n" + error);
}

void IceImplement::createRootSignature()
{
    // ルート引数 (シェーダーへ渡す物の一覧) は 3 つ:
    //   0: b0 シーンの定数 — カメラとライト。 エンジンが毎フレーム書いた定数バッファを、 そのまま結び付ける (CBV)
    //   1: b1 IceObject   — マテリアル・位置・SDF の箱など、 氷ごとの値。 32 bit 定数を直接置く
    //   2: t0 SDF         — SDF の 3D テクスチャ。 SRV はヒープに置くので、 デスクリプターテーブルで指す
    D3D12_DESCRIPTOR_RANGE sdfRange = {};
    sdfRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    sdfRange.NumDescriptors = 1;
    sdfRange.BaseShaderRegister = 0;  // t0
    sdfRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER params[3] = {};
    params[kRootScene].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[kRootScene].Descriptor.ShaderRegister = 0;
    params[kRootScene].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;  // 頂点とピクセルの両方で使う

    params[kRootObject].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[kRootObject].Constants.ShaderRegister = 1;
    params[kRootObject].Constants.Num32BitValues = sizeof(IceObjectConstants) / 4;
    params[kRootObject].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    params[kRootSdf].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[kRootSdf].DescriptorTable.NumDescriptorRanges = 1;
    params[kRootSdf].DescriptorTable.pDescriptorRanges = &sdfRange;
    params[kRootSdf].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 静的サンプラー (s0): ルートシグネチャに埋め込む、 変わらないサンプラー。
    // 線形補間 (格子の点の間をなめらかにつなぐ) と、 端で止める (箱の外を読んでも端の値になる)
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;  // s0
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 3;
    desc.pParameters = params;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
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
    // 2 つとも作れたときだけ差し替える。 読み直しに失敗しても、 前の PSO で描き続けられる
    ComPtr<ID3D12PipelineState> mesh, sdf;
    // プリミティブ: 裏面をカリング (表面だけ描く)。 深度に書く
    if (!createPipeline(mShaderDir + L"/IceMesh.hlsl", D3D12_CULL_MODE_BACK, true, mesh, error)) return false;
    // SDF: 箱の表面をカリングして裏面だけ描く (カメラが箱の中に入っても描けるように)。
    //      深度には書かない (箱の面の深さは氷の表面の深さではないため)
    if (!createPipeline(mShaderDir + L"/IceSdf.hlsl", D3D12_CULL_MODE_FRONT, false, sdf, error)) return false;
    mMeshPipeline = mesh;
    mSdfPipeline = sdf;
    return true;
}

bool IceImplement::createPipeline(const std::wstring& file, D3D12_CULL_MODE cull, bool depthWrite,
                                  ComPtr<ID3D12PipelineState>& pipeline, std::string& error)
{
    // 1. シェーダーをコンパイルする
    std::string log;
    ComPtr<ID3DBlob> vs = ShaderCompiler::compile(file, "VSMain", "vs_5_0", log);
    ComPtr<ID3DBlob> ps;
    if (vs) ps = ShaderCompiler::compile(file, "PSMain", "ps_5_0", log);
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

    // 形の三角形は 「外から見て反時計回り」 に並べてあるので、 反時計回りを表にする
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = cull;
    desc.RasterizerState.FrontCounterClockwise = TRUE;
    desc.RasterizerState.DepthClipEnable = TRUE;

    // 深度テスト: 手前の物だけを残す (深度が小さいほど手前)
    const D3D12_DEPTH_STENCILOP_DESC keep = {
        D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    desc.DepthStencilState.DepthEnable = TRUE;
    desc.DepthStencilState.DepthWriteMask = depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    desc.DepthStencilState.FrontFace = keep;
    desc.DepthStencilState.BackFace = keep;

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = mColorFormat;
    desc.DSVFormat = mDepthFormat;
    desc.SampleDesc.Count = 1;

    // 3. 作る
    const HRESULT hr = mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline));
    if (FAILED(hr)) {
        char text[128];
        std::snprintf(text, sizeof(text), "CreateGraphicsPipelineState(ice) failed (HRESULT 0x%08X)",
                      static_cast<unsigned>(hr));
        error = text;
        return false;
    }
    return true;
}

bool IceImplement::reloadShaders(std::string& error)
{
    return buildPipelines(error);
}

void IceImplement::draw3D(const DrawContext& context, const Float3& position)
{
    ID3D12GraphicsCommandList* commandList = context.commandList;

    // 1. 最初の 1 回だけ、 全部の形を作って mGeom に入れる (IceGenerate.cpp)。
    //    SDF の 3D テクスチャへ写す命令は、 このコマンドリストに記録されるので、 下の描画より先に GPU で実行される
    if (!mGenerated) {
        generateShape(commandList);
        mGenerated = true;
    }

    // 2. 今の形を選ぶ。 形の種類がそのまま mGeom の添え字になる
    const IceGeometry& geom = mGeom[static_cast<int>(mParams.shape)];

    // 3. 氷ごとの値 (カメラとライトはエンジンのシーンの定数をそのまま使う)
    IceObjectConstants object = {};
    object.material = mParams.material;
    object.position = position;
    object.boundsMin = geom.boundsMin;
    object.boundsMax = geom.boundsMax;
    object.voxelSize = geom.voxelSize;
    object.refractiveIndex = mParams.refractiveIndex;
    // 吸収の係数 σa = −ln(色) / 1 m。 Beer–Lambert の法則 T = exp(−σa × 厚さ) で、 厚さ 1 m のとき T = 色 になる
    const Float3& c = mParams.absorptionColor;
    object.absorption = { -std::log(c.x), -std::log(c.y), -std::log(c.z) };

    // 4. 氷のパイプラインに切り替えて描く。 描画先・ビューポート・シザーはエンジンが設定済み
    commandList->SetGraphicsRootSignature(mRootSignature.Get());
    commandList->SetPipelineState(geom.isSdf ? mSdfPipeline.Get() : mMeshPipeline.Get());
    commandList->SetGraphicsRootConstantBufferView(kRootScene, context.sceneConstants);
    commandList->SetGraphicsRoot32BitConstants(kRootObject, sizeof(object) / 4, &object, 0);
    if (geom.isSdf) {
        // SDF の 3D テクスチャ: ヒープをコマンドリストに設定し、 テーブル (t0) にヒープの先頭を指させる
        ID3D12DescriptorHeap* heaps[] = { mSrvHeap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);
        commandList->SetGraphicsRootDescriptorTable(kRootSdf, mSrvHeap->GetGPUDescriptorHandleForHeapStart());
    }
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &geom.vertexView);
    commandList->IASetIndexBuffer(&geom.indexView);
    commandList->DrawIndexedInstanced(geom.indexCount, 1, 0, 0, 0);
}
