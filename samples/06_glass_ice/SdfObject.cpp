#include "SdfObject.h"

#include "common/GpuHelpers.h"

#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

// GlassIce.hlsl の cbuffer ObjectConstants と同じ並び (32 bit × 44 個)
struct ObjectConstants {
    Float4x4 world;      // 部品の空間 → ワールド (転置して渡す)
    Float4x4 invWorld;   // ワールド → 部品の空間 (転置して渡す)
    Float3 cameraPos;
    uint32_t shape;
    Float3 absorption;
    float ior;
    float coreDensity;
    uint32_t thin;
    float pad[2];
};
static_assert(sizeof(ObjectConstants) == 44 * 4, "ObjectConstants must be 44 32-bit values");

struct BoxVertex {
    Float3 position;
    Float2 uv;
};

// 形を囲む箱の半分の大きさ。 GlassIce.hlsl の形より少し大きくする (ICE_HALF + でこぼこ、 CUP_R / CUP_H + 壁の厚さ)
Float3 boundsHalf(SdfObject::Shape shape)
{
    if (shape == SdfObject::Shape::IceCube) return { 0.15f, 0.15f, 0.15f };
    return { 0.38f, 0.42f, 0.38f };
}

// 箱の 6 面。 どの面も外から見て反時計回り (u × v = 法線 になる u, v で四角を作る)
void makeBox(const Float3& half, std::vector<BoxVertex>& vertices, std::vector<uint16_t>& indices)
{
    const Float3 axes[3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
    for (int axis = 0; axis < 3; ++axis) {
        for (int sign = -1; sign <= 1; sign += 2) {
            const Float3 n = axes[axis] * static_cast<float>(sign);
            // 表 (+) の面は u = 次の軸、 v = その次の軸。 裏 (−) の面は入れ替えると u × v = 法線 のまま
            const Float3 u = sign > 0 ? axes[(axis + 1) % 3] : axes[(axis + 2) % 3];
            const Float3 v = sign > 0 ? axes[(axis + 2) % 3] : axes[(axis + 1) % 3];
            const uint16_t base = static_cast<uint16_t>(vertices.size());
            const float corners[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
            for (const auto& c : corners) {
                const Float3 p = n + u * c[0] + v * c[1];
                vertices.push_back({ { p.x * half.x, p.y * half.y, p.z * half.z },
                                     { c[0] * 0.5f + 0.5f, 0.5f - c[1] * 0.5f } });
            }
            indices.insert(indices.end(), { base, static_cast<uint16_t>(base + 1), static_cast<uint16_t>(base + 2),
                                            base, static_cast<uint16_t>(base + 2), static_cast<uint16_t>(base + 3) });
        }
    }
}

// 行ベクトル (p × M) の流儀の回転と移動 (MathTypes.h と同じ流儀)
Float4x4 rotationY(float a)
{
    Float4x4 r = identity();
    r.m[0][0] = std::cos(a);
    r.m[0][2] = -std::sin(a);
    r.m[2][0] = std::sin(a);
    r.m[2][2] = std::cos(a);
    return r;
}

Float4x4 rotationZ(float a)
{
    Float4x4 r = identity();
    r.m[0][0] = std::cos(a);
    r.m[0][1] = std::sin(a);
    r.m[1][0] = -std::sin(a);
    r.m[1][1] = std::cos(a);
    return r;
}

Float4x4 translation(const Float3& t)
{
    Float4x4 r = identity();
    r.m[3][0] = t.x;
    r.m[3][1] = t.y;
    r.m[3][2] = t.z;
    return r;
}

} // namespace

SdfObject::SdfObject(std::wstring shaderFile, Shape shape)
    : mShaderFile(std::move(shaderFile)), mShape(shape)
{
}

void SdfObject::setRotation(float yaw, float tilt)
{
    mYaw = yaw;
    mTilt = tilt;
}

Float4x4 SdfObject::worldMatrix() const
{
    // 行ベクトルなので左から順にかかる: z 軸まわりに傾ける → y 軸まわりに向きを回す → 位置へ動かす
    return rotationZ(mTilt) * rotationY(mYaw) * translation(getPosition());
}

void SdfObject::initializeImplement(const RenderSetup& setup)
{
    mDevice = setup.device;
    mSetup = setup;

    // ルート引数: 0 = b0 エンジンのシーンの定数 (ViewProj・太陽)、 1 = b1 この部品の定数
    D3D12_ROOT_PARAMETER params[2] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[1].Constants.ShaderRegister = 1;
    params[1].Constants.Num32BitValues = sizeof(ObjectConstants) / 4;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 2;
    desc.pParameters = params;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;  // 頂点バッファを使う
    mRootSignature = gpu::createRootSignature(mDevice, desc);

    std::string error;
    if (!reloadImplement(error)) throw std::runtime_error("Sample shader error:\n" + error);

    // 形を囲む箱
    std::vector<BoxVertex> vertices;
    std::vector<uint16_t> indices;
    makeBox(boundsHalf(mShape), vertices, indices);
    const UINT vertexBytes = static_cast<UINT>(vertices.size() * sizeof(BoxVertex));
    const UINT indexBytes = static_cast<UINT>(indices.size() * sizeof(uint16_t));
    mVertexBuffer = gpu::createUploadBuffer(mDevice, vertices.data(), vertexBytes);
    mIndexBuffer = gpu::createUploadBuffer(mDevice, indices.data(), indexBytes);
    mVertexView = { mVertexBuffer->GetGPUVirtualAddress(), vertexBytes, sizeof(BoxVertex) };
    mIndexView = { mIndexBuffer->GetGPUVirtualAddress(), indexBytes, DXGI_FORMAT_R16_UINT };
    mIndexCount = static_cast<UINT>(indices.size());
}

bool SdfObject::reloadImplement(std::string& error)
{
    static const D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    gpu::PipelineOptions options;
    options.inputLayout = layout;
    options.inputCount = 2;
    // 箱の裏面を描く: カメラが箱の中に入っても、 箱が消えない。 深度はシェーダーが当たった点の物を出す
    options.cull = D3D12_CULL_MODE_FRONT;
    // 半透明で重ねる (氷は α = 1 で上書き、 コップは映り込み + 後ろの色 × 透過率)
    options.blend = true;
    return gpu::createPipeline(mDevice, mRootSignature.Get(), mSetup, mShaderFile, options, mPipeline, error);
}

void SdfObject::drawImplement(const DrawContext& context)
{
    const Float4x4 world = worldMatrix();
    ObjectConstants constants = {};
    constants.world = transpose(world);  // HLSL は列優先で読むので転置して渡す (ViewProj と同じ)
    constants.invWorld = transpose(inverse(world));
    constants.cameraPos = context.scene.cameraPos;
    constants.shape = static_cast<uint32_t>(mShape);
    constants.absorption = mAbsorption;
    constants.ior = mIor;
    constants.coreDensity = mCoreDensity;
    constants.thin = mShape == Shape::Cup ? 1u : 0u;

    ID3D12GraphicsCommandList* commandList = context.commandList;
    commandList->SetGraphicsRootSignature(mRootSignature.Get());
    commandList->SetPipelineState(mPipeline.Get());
    commandList->SetGraphicsRootConstantBufferView(0, context.sceneConstants);
    commandList->SetGraphicsRoot32BitConstants(1, sizeof(ObjectConstants) / 4, &constants, 0);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &mVertexView);
    commandList->IASetIndexBuffer(&mIndexView);
    commandList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
}
