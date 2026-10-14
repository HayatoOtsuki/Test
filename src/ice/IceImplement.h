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

    // ---- SDF の形のときだけ使う ----
    bool isSdf = false;                                   // true なら SDF 用の PSO で描く
    Microsoft::WRL::ComPtr<ID3D12Resource> sdfTexture;    // SDF の 3D テクスチャ (GPU 専用のメモリ)
    Microsoft::WRL::ComPtr<ID3D12Resource> sdfUpload;     // 3D テクスチャへ送るための一時バッファ (写し終えるまで残す)
    Float3 boundsMin;                                     // 格子が覆う箱の最小の角 (物体の空間)
    Float3 boundsMax;                                     // 同じく最大の角。 頂点はこの箱のメッシュ
    float voxelSize = 0.0f;                               // 格子 1 つ分の大きさ (メートル)
};

// 氷の値。 IceComponent のセッター・ゲッターが読み書きする
// (位置は IceComponent が継承した RenderEntity が持つので、 ここには無い)
struct IceParams {
    IceShape shape = IceShape::Cube;

    // プリミティブの質感。 初めは淡い水色、 金属でない、 少しつるつる
    Material material = { { 0.70f, 0.85f, 0.95f, 1.0f }, 0.0f, 0.3f, 0.5f };

    // SDF の氷
    float refractiveIndex = 1.31f;                   // 氷の屈折率
    Float3 absorptionColor = { 0.45f, 0.75f, 0.90f };  // 厚さ 1 m を通った白がこの色になる (本物より強め)
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
    // HLSL のファイル 1 つから PSO を 1 つ作る。 cull: カリングする面、 depthWrite: 深度に書くか
    bool createPipeline(const std::wstring& file, D3D12_CULL_MODE cull, bool depthWrite,
                        Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipeline, std::string& error);

    // ---- IceGenerate.cpp: 形づくり ----
    // commandList: SDF の 3D テクスチャへ写す命令を記録する先 (draw3D のコマンドリスト)
    void generateShape(ID3D12GraphicsCommandList* commandList);
    IceGeometry generateCube();
    IceGeometry generateSphere();
    IceGeometry generateCylinder();
    IceGeometry generateSDF(ID3D12GraphicsCommandList* commandList);
    // 頂点とインデックスの並びから、 GPU のバッファを作って IceGeometry にまとめる
    IceGeometry createGeometry(const std::vector<IceVertex>& vertices, const std::vector<uint32_t>& indices);

    ID3D12Device* mDevice = nullptr;
    DXGI_FORMAT mColorFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT mDepthFormat = DXGI_FORMAT_UNKNOWN;
    std::wstring mShaderDir;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mMeshPipeline;  // プリミティブ (メッシュ) 用
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mSdfPipeline;   // SDF の形用 (箱の中をレイマーチ)
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;      // SDF の 3D テクスチャの SRV を置くヒープ (シェーダーから見える)

    IceGeometry mGeom[static_cast<int>(IceShape::Count)];  // 全部の形
    bool mGenerated = false;                               // generateShape を済ませたか
    IceParams mParams;
};
