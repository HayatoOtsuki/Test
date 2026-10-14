#pragma once
// =============================================================================
// IceImplement.h — 氷の部品の実装
//
// IceComponent が private に持つ。 描く仕組みの関数 (initialize / draw3D / reloadShaders) は、 すべてこのクラスにある。
// Renderer は IceComponent (RenderEntity) を呼び、 IceComponent がここへ渡す。
//
// 中身は 2 つのファイルに分けてある:
//   IceImplement.cpp : 描く準備 (ルートシグネチャ・パイプライン) と draw3D
//   IceGenerate.cpp  : 形づくり (generateShape と、 形ごとの generateXxx)
//
// 形はすべて最初の draw3D で 1 回だけ作り、 mGeom (形の種類を添え字にした配列) に入れておく。
// 描くときは mGeom[今の形] を選ぶだけなので、 形の切り替えは一瞬で済む。
// =============================================================================

#include "IceComponent.h"

#include "engine/DrawContext.h"
#include "engine/Material.h"
#include "engine/MathTypes.h"
#include "engine/RenderEntity.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>

// 頂点 1 つ。 IceMesh.hlsl の VSInput と同じ並び (32 バイト)
struct IceVertex {
    Float3 position;  // 物体の空間の位置 (原点は底の真ん中、 メートル)
    Float3 normal;    // 外向きの法線 (長さ 1)
    Float2 uv;        // テクスチャ座標 (0〜1)
};

// 形 1 つ分 (mGeom の 1 つ)
struct IceGeometry {
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexView = {};  // 頂点バッファのどこからどれだけ読むか
    D3D12_INDEX_BUFFER_VIEW indexView = {};    // インデックスバッファの同じもの
    UINT indexCount = 0;                       // 描くインデックスの数 (三角形の数 × 3)
};

// 氷の値。 IceComponent のセッター・ゲッターが読み書きする
// (位置は IceComponent が継承した RenderEntity が持つので、 ここには無い)
struct IceParams {
    IceShape shape = IceShape::Cube;

    // プリミティブの質感。 初めは淡い水色、 金属でない、 少しつるつる
    Material material = { { 0.70f, 0.85f, 0.95f, 1.0f }, 0.0f, 0.3f, 0.5f };
};

class IceImplement {
public:
    // shaderDir: 氷のシェーダーのフォルダ (src/ice/shaders)
    explicit IceImplement(std::wstring shaderDir);

    // ---- IceComponent (Renderer から呼ばれる関数) から呼ばれる ----

    // 描く準備。 ルートシグネチャとパイプラインを作る (登録のときに 1 回)
    void initialize(const RenderSetup& setup);
    // 毎フレーム呼ばれる。 最初の 1 回で全部の形を作り、 今の形 (mParams.shape) を position に描く
    void draw3D(const DrawContext& context, const Float3& position);
    // シェーダーを読み直す。 失敗したら false と理由を返し、 前のシェーダーで描き続ける
    bool reloadShaders(std::string& error);

    // ---- IceComponent のセッター・ゲッターが使う ----

    IceParams& getParams() { return mParams; }
    const IceParams& getParams() const { return mParams; }

private:
    // ---- IceImplement.cpp: 描く準備 ----
    void createRootSignature();
    bool buildPipelines(std::string& error);

    // ---- IceGenerate.cpp: 形づくり ----
    void generateShape();
    IceGeometry generateCube();
    IceGeometry generateSphere();
    IceGeometry generateCylinder();
    // 頂点とインデックスの並びから、 GPU のバッファを作って IceGeometry にまとめる
    IceGeometry createGeometry(const std::vector<IceVertex>& vertices, const std::vector<uint32_t>& indices);

    ID3D12Device* mDevice = nullptr;
    DXGI_FORMAT mColorFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT mDepthFormat = DXGI_FORMAT_UNKNOWN;
    std::wstring mShaderDir;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mMeshPipeline;  // プリミティブ (メッシュ) 用

    IceGeometry mGeom[static_cast<int>(IceShape::Count)];  // 全部の形
    bool mGenerated = false;                               // generateShape を済ませたか
    IceParams mParams;
};
