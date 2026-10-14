#pragma once
// =============================================================================
// DepthBuffer.h — 深度バッファ
//
// 画素ごとに 「いちばん手前の物までの奥行き」 を覚えておくテクスチャ。
// 立体の形を描くとき、 奥の面が手前の面を上書きしないようにする (深度テスト)。
// =============================================================================

#include <d3d12.h>
#include <wrl/client.h>

class DepthBuffer {
public:
    // 32 bit の浮動小数の深度 (0 = 手前、 1 = 奥)。 ステンシルは使わない
    static constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_D32_FLOAT;

    DepthBuffer(ID3D12Device* device, UINT width, UINT height);

    DepthBuffer(const DepthBuffer&) = delete;
    DepthBuffer& operator=(const DepthBuffer&) = delete;

    // 大きさを変える (作り直す)。 GPU が使っていないときに呼ぶこと
    void resize(UINT width, UINT height);

    // 描画先として指すための DSV (デプスステンシルビュー)
    D3D12_CPU_DESCRIPTOR_HANDLE getDsv() const;

private:
    void create(UINT width, UINT height);

    ID3D12Device* mDevice;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;  // DSV の置き場 (1 つ)
    Microsoft::WRL::ComPtr<ID3D12Resource> mTexture;
    UINT mWidth = 0;
    UINT mHeight = 0;
};
