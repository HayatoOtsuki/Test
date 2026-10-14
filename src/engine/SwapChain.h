#pragma once
// =============================================================================
// SwapChain.h — ウィンドウに絵を出すためのバックバッファの組
//
// バックバッファを 2 枚持ち、 交互に使う (1 枚に描いている間、 もう 1 枚を表示する)。
// それぞれに RTV (レンダーターゲットビュー = 描画先として指すための記述子) を作っておく。
// =============================================================================

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

class GraphicsDevice;

class SwapChain {
public:
    static constexpr UINT kBufferCount = 2;
    // 8 bit × RGBA。 sRGB への変換 (ガンマ) はシェーダーの最後で自分で掛ける
    static constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    SwapChain(GraphicsDevice& device, HWND hwnd, UINT width, UINT height);

    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    // バックバッファの大きさを変える。 GPU がバックバッファを使っていないときに呼ぶこと
    void resize(UINT width, UINT height);

    // 描き終えたバックバッファを表示に回す (垂直同期を 1 回待つ)
    void present();

    // 今回描くバックバッファと、 その RTV
    ID3D12Resource* getCurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE getCurrentRtv() const;

    UINT getWidth() const { return mWidth; }
    UINT getHeight() const { return mHeight; }

private:
    // バックバッファを取り出し、 それぞれの RTV をヒープに作る
    void createRenderTargetViews();

    GraphicsDevice& mDevice;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;  // RTV の置き場
    Microsoft::WRL::ComPtr<ID3D12Resource> mBuffers[kBufferCount];
    UINT mRtvStride = 0;  // ヒープの中の RTV 1 つ分の大きさ (GPU によって違う)
    UINT mWidth = 0;
    UINT mHeight = 0;
};
