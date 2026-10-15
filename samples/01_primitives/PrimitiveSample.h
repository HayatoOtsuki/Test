#pragma once
// =============================================================================
// PrimitiveSample.h — サンプル 01: プリミティブだけ
//
// キューブ・球・円柱のメッシュ (三角形の集まり) を作って GPU へ送り、 ランバートで描く。
//   1 / 2 / 3: 形   W: ワイヤーフレーム (三角形の辺だけ)   N: 色の見方 (光 → 法線 → UV)
//
// メッシュが持つのは 「表面の三角形」 だけ。 中身 (中か外か、 表面からの深さ) は持たない。
// =============================================================================

#include "PrimitiveMesh.h"

#include "common/SampleEntity.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <string>

class PrimitiveSample : public SampleEntity {
public:
    explicit PrimitiveSample(std::wstring shaderDir);

    void onKey(WPARAM key) override;
    std::wstring getStatus() const override;

private:
    // ---- RenderEntity: Renderer から呼ばれる ----
    void initializeImplement(const RenderSetup& setup) override;
    void drawImplement(const DrawContext& context) override;
    bool reloadImplement(std::string& error) override;

    // GPU に置いた形 1 つ分
    struct GpuMesh {
        Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW vertexView = {};
        D3D12_INDEX_BUFFER_VIEW indexView = {};
        UINT indexCount = 0;
    };
    GpuMesh upload(const MeshData& mesh);   // 頂点とインデックスを GPU のバッファにする
    bool buildPipelines(std::string& error);

    std::wstring mShaderDir;
    ID3D12Device* mDevice = nullptr;
    RenderSetup mSetup;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mSolidPipeline;  // 面を塗る
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mWirePipeline;   // 三角形の辺だけ描く
    GpuMesh mMeshes[3];  // 0 = キューブ、 1 = 球、 2 = 円柱
    int mShape = 0;
    bool mWireframe = false;
    int mColorMode = 0;  // 0 = 光、 1 = 法線、 2 = UV
};
