#include "SceneColorBuffer.h"

#include "DxCheck.h"

SceneColorBuffer::SceneColorBuffer(ID3D12Device* device, UINT width, UINT height) : mDevice(device)
{
    // RTV を 1 つ入れるヒープ
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = 1;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&mRtvHeap)), "CreateDescriptorHeap(scene RTV)");

    // SRV を 1 つ入れるヒープ。 描画中にシェーダーから読むので、 シェーダーから見える (SHADER_VISIBLE) 物にする
    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = 1;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&mSrvHeap)), "CreateDescriptorHeap(scene SRV)");

    create(width, height);
}

void SceneColorBuffer::resize(UINT width, UINT height)
{
    if (width == 0 || height == 0 || (width == mWidth && height == mHeight)) return;
    create(width, height);
}

void SceneColorBuffer::create(UINT width, UINT height)
{
    mWidth = width;
    mHeight = height;

    // GPU 専用のメモリ (既定ヒープ) に置く
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
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;  // 描画先にする

    // 最初は 「シェーダーから読む」 状態で作る。 Renderer は毎フレーム、 描く前に描画先の状態へ移し、 描いた後に戻す。
    // 背景が全部の画素を塗るのでクリアはしない (クリアの値も渡さない)
    mTexture.Reset();
    ThrowIfFailed(mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
                                                   IID_PPV_ARGS(&mTexture)),
                  "CreateCommittedResource(scene color)");

    // 記述 (desc) に nullptr を渡すと、 リソースの形式どおりのビューになる
    mDevice->CreateRenderTargetView(mTexture.Get(), nullptr, getRtv());
    mDevice->CreateShaderResourceView(mTexture.Get(), nullptr, mSrvHeap->GetCPUDescriptorHandleForHeapStart());
}

D3D12_CPU_DESCRIPTOR_HANDLE SceneColorBuffer::getRtv() const
{
    return mRtvHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE SceneColorBuffer::getSrv() const
{
    return mSrvHeap->GetGPUDescriptorHandleForHeapStart();
}
