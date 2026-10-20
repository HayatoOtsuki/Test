#pragma once
// =============================================================================
// EnvironmentMap.h — 環境マップ (キューブマップ) を GPU へ送る
//
// EnvironmentImage が CPU で作った 6 面を、 キューブマップのテクスチャにする。 氷のシェーダーは t0 でこれを読む。
// 太陽の向きか色が変わったら (J / L で太陽を回したとき) 作り直す。
// 同じヒープの次の場所 (t1) に、 エンジンがくれる画面のコピー (周りの景色) の SRV も置く (段階 13 から使う)。
// 自分のエンジンでは、 スカイボックスのキューブマップをそのまま t0 に渡せばよい (このクラスは要らない)。
// =============================================================================

#include "engine/MathTypes.h"

#include <d3d12.h>
#include <wrl/client.h>

class EnvironmentMap {
public:
    static constexpr int kSize = 128;  // 1 面の 1 辺の画素の数

    // SRV を置くヒープを作る
    void initialize(ID3D12Device* device);

    // 太陽の向きか色が前と違えば、 6 面を作り直して、 写す命令を commandList に記録する。
    // 前のテクスチャを GPU が使い終わっていること (Renderer は毎フレーム GPU を待つので、 drawImplement の中なら良い)
    void update(ID3D12GraphicsCommandList* commandList, const Float3& lightDirection, const Float3& lightColor);

    // 画面のコピー (DrawContext::sceneCopy) の SRV を、 t1 の場所に作る。 毎フレーム呼んでよい (大きさが変わっても追いつく)
    void setSceneCopy(ID3D12Resource* sceneCopy);

    ID3D12DescriptorHeap* getHeap() const { return mHeap.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE getSrv() const { return mHeap->GetGPUDescriptorHandleForHeapStart(); }  // t0, t1 の並びの先頭

private:
    ID3D12Device* mDevice = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> mTexture;
    Microsoft::WRL::ComPtr<ID3D12Resource> mUpload;  // 写し終わるまで (このフレームの GPU の実行が終わるまで) 持っておく
    bool mValid = false;                             // 1 回でも作ったか
    Float3 mLastDirection;                           // 作ったときの太陽の向きと色
    Float3 mLastColor;
};
