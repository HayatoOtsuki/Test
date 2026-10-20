#pragma once
// =============================================================================
// SdfObject.h — SDF の形を 1 つ描く部品 (コンポーネント 1 つ = 形 1 つ)
//
// 形を囲む箱のメッシュを描き、 箱が映る画素でシェーダー (IceShaderN.hlsl) が光線を追って形を描く。
// 位置は RenderEntity の位置。 回転・屈折率・吸収・白い芯をセッターで決める。
// 氷を 3 つ出すなら、 この部品を 3 つ置くだけ。
// =============================================================================

#include "EnvironmentMap.h"

#include "engine/MathTypes.h"
#include "engine/RenderEntity.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>

class SdfObject : public RenderEntity {
public:
    // シェーダーの shape と同じ番号
    enum class Shape : uint32_t { IceCube = 0, Cup = 1 };

    SdfObject(std::wstring shaderFile, Shape shape);

    void setRotation(float yaw, float tilt);           // z 軸まわりに tilt 傾けてから、 y 軸まわりに yaw 回す
    void setRefractiveIndex(float ior) { mIor = ior; } // 屈折率 (氷 1.31、 ガラス 1.5)
    void setAbsorption(const Float3& sigma) { mAbsorption = sigma; }  // 1 m あたりの吸収 (赤・緑・青)
    void setCoreDensity(float density) { mCoreDensity = density; }    // 白い芯の濃さ (1/m)。 0 なら芯なし
    void setVisible(bool visible) { mVisible = visible; }             // false なら描かない
    void setEnvironmentMap(const EnvironmentMap* map) { mEnvironment = map; }  // 周りの景色 (t0 のキューブマップ)

    // 使うシェーダーを変える。 次に描くときにコンパイルし直す (失敗したら前のシェーダーのまま)
    void setShaderFile(const std::wstring& shaderFile);

private:
    void initializeImplement(const RenderSetup& setup) override;
    void drawImplement(const DrawContext& context) override;
    bool reloadImplement(std::string& error) override;

    Float4x4 worldMatrix() const;  // 傾き → 向き → 位置

    std::wstring mShaderFile;
    Shape mShape;
    float mYaw = 0.0f;
    float mTilt = 0.0f;
    float mIor = 1.31f;
    Float3 mAbsorption = { 0.10f, 0.04f, 0.02f };
    float mCoreDensity = 0.0f;
    bool mVisible = true;
    const EnvironmentMap* mEnvironment = nullptr;
    bool mShaderChanged = false;  // setShaderFile の後、 まだコンパイルし直していない

    ID3D12Device* mDevice = nullptr;
    RenderSetup mSetup;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipeline;
    Microsoft::WRL::ComPtr<ID3D12Resource> mVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> mIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW mVertexView = {};
    D3D12_INDEX_BUFFER_VIEW mIndexView = {};
    UINT mIndexCount = 0;
};
