#pragma once
// =============================================================================
// SdfTexture.h — 焼いた格子 (SdfGrid) を GPU の 3D テクスチャにする
//
// 手順は本体の generateSDF の 3〜6 と同じ (docs/05-generate-sdf.md §4):
//   1. 3D テクスチャを作る (GPU 専用のメモリ、 R32_FLOAT)
//   2. 一時バッファ (アップロードヒープ) に、 GPU の決まりの並び (行を 256 バイトの倍数) で書く
//   3. 写す命令と、 「読む状態」 へ変える命令をコマンドリストに記録する
//   4. シェーダーから読むための SRV を、 シェーダーから見えるヒープに作る
// =============================================================================

#include "SdfBaker.h"

#include <d3d12.h>
#include <wrl/client.h>

class SdfTexture {
public:
    // SRV を 1 つ置くヒープを作る
    void initialize(ID3D12Device* device);

    // grid を新しい 3D テクスチャにして、 写す命令を commandList に記録する。 前のテクスチャは捨てる。
    // 前のテクスチャを GPU が使い終わっていること (このサンプルの Renderer は毎フレーム GPU を待つので、
    // drawImplement の中で呼べば良い)
    void upload(ID3D12GraphicsCommandList* commandList, const SdfGrid& grid);

    ID3D12DescriptorHeap* getHeap() const { return mHeap.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE getSrv() const { return mHeap->GetGPUDescriptorHandleForHeapStart(); }

private:
    ID3D12Device* mDevice = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> mTexture;
    // 写す命令は記録しただけで、 GPU で実行されるのはこのフレームの最後。 それまで壊さないよう持っておく
    Microsoft::WRL::ComPtr<ID3D12Resource> mUpload;
};
