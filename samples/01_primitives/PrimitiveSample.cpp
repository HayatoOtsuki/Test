#include "PrimitiveSample.h"

#include "common/GpuHelpers.h"

#include <stdexcept>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace {

// Primitive.hlsl の cbuffer PrimitiveConstants (b1) と同じ並び (32 bit × 4 個)
struct PrimitiveConstants {
    Float3 baseColor;  // 面の色
    uint32_t colorMode;  // 0 = 光、 1 = 法線、 2 = UV
};
static_assert(sizeof(PrimitiveConstants) == 4 * 4, "PrimitiveConstants must be four 32-bit values");

const wchar_t* const kShapeNames[] = { L"キューブ", L"球", L"円柱" };
const wchar_t* const kColorModeNames[] = { L"光", L"法線", L"UV" };

} // namespace

PrimitiveSample::PrimitiveSample(std::wstring shaderDir) : mShaderDir(std::move(shaderDir))
{
}

void PrimitiveSample::initializeImplement(const RenderSetup& setup)
{
    mDevice = setup.device;
    mSetup = setup;

    // ルート引数: 0 = b0 エンジンのシーンの定数 (カメラ・ライト)、 1 = b1 このサンプルの定数
    D3D12_ROOT_PARAMETER params[2] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[1].Constants.ShaderRegister = 1;
    params[1].Constants.Num32BitValues = sizeof(PrimitiveConstants) / 4;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 2;
    desc.pParameters = params;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;  // 頂点バッファを使う
    mRootSignature = gpu::createRootSignature(mDevice, desc);

    std::string error;
    if (!buildPipelines(error)) throw std::runtime_error("Sample shader error:\n" + error);

    // 形を作って GPU へ送る (作るのはこの 1 回だけ。 切り替えは選ぶだけ)
    mMeshes[0] = upload(primitive::makeCube(0.4f));
    mMeshes[1] = upload(primitive::makeSphere(0.42f, 32, 16));
    mMeshes[2] = upload(primitive::makeCylinder(0.35f, 0.8f, 32));
}

bool PrimitiveSample::buildPipelines(std::string& error)
{
    // 頂点の並び (MeshVertex と同じ)
    static const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    gpu::PipelineOptions options;
    options.inputLayout = inputLayout;
    options.inputCount = 3;

    const std::wstring file = mShaderDir + L"/Primitive.hlsl";
    ComPtr<ID3D12PipelineState> solid, wire;
    if (!gpu::createPipeline(mDevice, mRootSignature.Get(), mSetup, file, options, solid, error)) return false;
    // ワイヤーフレーム: 三角形を塗らずに辺だけ描く。 裏の三角形も見えるよう、 カリングしない
    options.fill = D3D12_FILL_MODE_WIREFRAME;
    options.cull = D3D12_CULL_MODE_NONE;
    if (!gpu::createPipeline(mDevice, mRootSignature.Get(), mSetup, file, options, wire, error)) return false;
    mSolidPipeline = solid;
    mWirePipeline = wire;
    return true;
}

bool PrimitiveSample::reloadImplement(std::string& error)
{
    return buildPipelines(error);
}

PrimitiveSample::GpuMesh PrimitiveSample::upload(const MeshData& mesh)
{
    // 頂点とインデックスはアップロードヒープに置いたまま使う (小さく、 作った後に変えないため)
    GpuMesh gpuMesh;
    const size_t vertexBytes = mesh.vertices.size() * sizeof(MeshVertex);
    const size_t indexBytes = mesh.indices.size() * sizeof(uint32_t);
    gpuMesh.vertexBuffer = gpu::createUploadBuffer(mDevice, mesh.vertices.data(), vertexBytes);
    gpuMesh.indexBuffer = gpu::createUploadBuffer(mDevice, mesh.indices.data(), indexBytes);
    gpuMesh.vertexView = { gpuMesh.vertexBuffer->GetGPUVirtualAddress(), static_cast<UINT>(vertexBytes),
                           static_cast<UINT>(sizeof(MeshVertex)) };
    gpuMesh.indexView = { gpuMesh.indexBuffer->GetGPUVirtualAddress(), static_cast<UINT>(indexBytes),
                          DXGI_FORMAT_R32_UINT };
    gpuMesh.indexCount = static_cast<UINT>(mesh.indices.size());
    return gpuMesh;
}

void PrimitiveSample::drawImplement(const DrawContext& context)
{
    const GpuMesh& mesh = mMeshes[mShape];
    PrimitiveConstants constants = {};
    constants.baseColor = { 0.70f, 0.85f, 0.95f };
    constants.colorMode = static_cast<uint32_t>(mColorMode);

    ID3D12GraphicsCommandList* commandList = context.commandList;
    commandList->SetGraphicsRootSignature(mRootSignature.Get());
    commandList->SetPipelineState(mWireframe ? mWirePipeline.Get() : mSolidPipeline.Get());
    commandList->SetGraphicsRootConstantBufferView(0, context.sceneConstants);
    commandList->SetGraphicsRoot32BitConstants(1, sizeof(constants) / 4, &constants, 0);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &mesh.vertexView);
    commandList->IASetIndexBuffer(&mesh.indexView);
    commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
}

void PrimitiveSample::onKey(WPARAM key)
{
    if (key >= '1' && key <= '3') mShape = static_cast<int>(key - '1');
    if (key == 'W') mWireframe = !mWireframe;
    if (key == 'N') mColorMode = (mColorMode + 1) % 3;
}

std::wstring PrimitiveSample::getStatus() const
{
    std::wstring status = std::wstring(L"形: ") + kShapeNames[mShape] + L" | 色: " + kColorModeNames[mColorMode];
    status += L" | 三角形: " + std::to_wstring(mMeshes[mShape].indexCount / 3);
    if (mWireframe) status += L" | ワイヤーフレーム";
    return status;
}
