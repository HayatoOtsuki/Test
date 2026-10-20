#pragma once
// =============================================================================
// SceneColorBuffer.h — シーンを描く描画先 (HDR、 明るさのまま)
//
// 背景も氷も、 まずここに 「光の強さ (明るさ) のまま」 描く。 1 を超える明るさ (太陽など) もそのまま入る。
// フレームの最後に DisplayPass がここを読み、 表示用の色 (ToDisplay: トーンマップとガンマ) に直してバックバッファへ写す。
// だから各シェーダーは、 表示用の変換をせずに float4(色, α) を返すだけでよい。
// 半透明を重ねるブレンドも、 明るさのまま (光の足し算として正しく) 行われる。
// =============================================================================

#include <d3d12.h>
#include <wrl/client.h>

class SceneColorBuffer {
public:
    // 16 bit の浮動小数 × 4 (赤・緑・青・α)。 1 を超える値や細かい暗さも入る
    static constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

    SceneColorBuffer(ID3D12Device* device, UINT width, UINT height);

    SceneColorBuffer(const SceneColorBuffer&) = delete;
    SceneColorBuffer& operator=(const SceneColorBuffer&) = delete;

    // 大きさを変える (作り直す)。 GPU が使っていないときに呼ぶこと
    void resize(UINT width, UINT height);

    ID3D12Resource* getResource() const { return mTexture.Get(); }

    // 描画先として指すための RTV (レンダーターゲットビュー)
    D3D12_CPU_DESCRIPTOR_HANDLE getRtv() const;

    // シェーダーから読むための SRV と、 それがあるヒープ (シェーダーから見える物)
    ID3D12DescriptorHeap* getSrvHeap() const { return mSrvHeap.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE getSrv() const;

private:
    void create(UINT width, UINT height);

    ID3D12Device* mDevice;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;  // RTV の置き場 (1 つ)
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;  // SRV の置き場 (1 つ、 シェーダーから見える)
    Microsoft::WRL::ComPtr<ID3D12Resource> mTexture;
    UINT mWidth = 0;
    UINT mHeight = 0;
};
