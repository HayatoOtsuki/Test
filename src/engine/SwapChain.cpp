#include "SwapChain.h"

#include "DxCheck.h"
#include "GraphicsDevice.h"

using Microsoft::WRL::ComPtr;

SwapChain::SwapChain(GraphicsDevice& device, HWND hwnd, UINT width, UINT height)
    : mDevice(device), mWidth(width), mHeight(height)
{
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = kFormat;
    desc.SampleDesc.Count = 1;  // MSAA は使わない (FLIP 系のスワップチェーンでは使えない)
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kBufferCount;
    // DirectX 12 では FLIP 系が必須。 DISCARD は 「表示した後の中身は残さなくてよい」
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    // DirectX 12 では、 スワップチェーンはデバイスでなくコマンドキューに結び付ける。
    // present がキューの命令と同じ列に並び、 描き終わった後に表示されるようにするため。
    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(device.getFactory()->CreateSwapChainForHwnd(device.getQueue(), hwnd, &desc, nullptr, nullptr,
                                                             &swapChain1),
                  "CreateSwapChainForHwnd");
    // 今のバックバッファの番号 (GetCurrentBackBufferIndex) を使うため、 IDXGISwapChain3 として持つ
    ThrowIfFailed(swapChain1.As(&mSwapChain), "QueryInterface(IDXGISwapChain3)");

    // Alt+Enter での全画面の切り替えは扱わないので止める
    device.getFactory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    // RTV を入れるヒープを、 バックバッファの数だけの大きさで作る
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = kBufferCount;
    ThrowIfFailed(device.getDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mRtvHeap)), "CreateDescriptorHeap");
    mRtvStride = device.getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    createRenderTargetViews();
}

void SwapChain::createRenderTargetViews()
{
    // ヒープの先頭から、 RTV 1 つ分ずつずらしながら作る
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kBufferCount; ++i) {
        ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mBuffers[i])), "GetBuffer");
        // 記述 (desc) に nullptr を渡すと、 リソースの形式どおりの RTV になる
        mDevice.getDevice()->CreateRenderTargetView(mBuffers[i].Get(), nullptr, rtv);
        rtv.ptr += mRtvStride;
    }
}

void SwapChain::resize(UINT width, UINT height)
{
    // 最小化中 (0 × 0) と、 大きさが変わっていないときは何もしない
    if (width == 0 || height == 0 || (width == mWidth && height == mHeight)) return;

    // ResizeBuffers の前に、 バックバッファへの参照を全部手放す必要がある
    for (auto& buffer : mBuffers) buffer.Reset();
    ThrowIfFailed(mSwapChain->ResizeBuffers(kBufferCount, width, height, kFormat, 0), "ResizeBuffers");
    mWidth = width;
    mHeight = height;
    createRenderTargetViews();
}

void SwapChain::present()
{
    // 第 1 引数 1: 垂直同期を 1 回待ってから表示する (画面の裂けを防ぎ、 速さを画面の更新に合わせる)
    ThrowIfFailed(mSwapChain->Present(1, 0), "Present");
}

ID3D12Resource* SwapChain::getCurrentBackBuffer() const
{
    return mBuffers[mSwapChain->GetCurrentBackBufferIndex()].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::getCurrentRtv() const
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += SIZE_T(mSwapChain->GetCurrentBackBufferIndex()) * mRtvStride;
    return rtv;
}
