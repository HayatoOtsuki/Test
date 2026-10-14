#include "DepthBuffer.h"

#include "DxCheck.h"

DepthBuffer::DepthBuffer(ID3D12Device* device, UINT width, UINT height) : mDevice(device)
{
    // DSV を 1 つ入れるヒープ
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heapDesc.NumDescriptors = 1;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDsvHeap)), "CreateDescriptorHeap(DSV)");

    create(width, height);
}

void DepthBuffer::resize(UINT width, UINT height)
{
    if (width == 0 || height == 0 || (width == mWidth && height == mHeight)) return;
    create(width, height);
}

void DepthBuffer::create(UINT width, UINT height)
{
    mWidth = width;
    mHeight = height;

    // GPU 専用のメモリ (既定ヒープ) に置く。 CPU からは触らない
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = kFormat;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;  // 並び方は GPU に任せる
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    // 毎フレーム 1.0 (いちばん奥) でクリアする。 同じ値を最適なクリア値として伝えると速くなる
    D3D12_CLEAR_VALUE clear = {};
    clear.Format = kFormat;
    clear.DepthStencil.Depth = 1.0f;

    mTexture.Reset();
    ThrowIfFailed(mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                   &clear, IID_PPV_ARGS(&mTexture)),
                  "CreateCommittedResource(depth)");

    // 記述 (desc) に nullptr を渡すと、 リソースの形式どおりの DSV になる
    mDevice->CreateDepthStencilView(mTexture.Get(), nullptr, getDsv());
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthBuffer::getDsv() const
{
    return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}
