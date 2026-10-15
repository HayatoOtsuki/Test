#include "SdfTexture.h"

#include "common/GpuHelpers.h"
#include "engine/DxCheck.h"

#include <cstdint>
#include <cstring>
#include <vector>

void SdfTexture::initialize(ID3D12Device* device)
{
    mDevice = device;

    // SRV は、 シェーダーから見える (SHADER_VISIBLE) ヒープに置かないと、 描画中に読めない
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mHeap)), "CreateDescriptorHeap(SDF SRV)");
}

void SdfTexture::upload(ID3D12GraphicsCommandList* commandList, const SdfGrid& grid)
{
    const UINT n = static_cast<UINT>(grid.size);

    // ---- 1. 3D テクスチャを作る (GPU 専用のメモリ = 既定ヒープ)。 最初は 「写される先」 の状態 ----
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    texDesc.Width = n;
    texDesc.Height = n;
    texDesc.DepthOrArraySize = static_cast<UINT16>(n);  // 3D テクスチャでは奥行き
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R32_FLOAT;  // 距離を 32 bit の浮動小数で 1 つ
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;  // メモリの並びは GPU に任せる

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    ThrowIfFailed(mDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
                                                   D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mTexture)),
                  "CreateCommittedResource(baked SDF texture)");

    // ---- 2. 一時バッファに、 GPU の決まりの並びで書く ----
    // バッファ側の 1 行 (RowPitch) は 256 バイトの倍数。 n = 8 なら 1 行は 32 バイトしか無いので、 後ろに隙間ができる。
    // GetCopyableFootprints が、 この決まりに合う並びを教えてくれる
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT rowCount = 0;
    UINT64 rowBytes = 0, totalBytes = 0;
    mDevice->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &rowCount, &rowBytes, &totalBytes);

    std::vector<uint8_t> staging(static_cast<size_t>(totalBytes));
    const UINT rowPitch = footprint.Footprint.RowPitch;
    const UINT64 slicePitch = static_cast<UINT64>(rowPitch) * rowCount;  // 1 枚 (z が同じ面) の大きさ
    for (UINT z = 0; z < n; ++z) {
        for (UINT y = 0; y < rowCount; ++y) {
            uint8_t* dst = staging.data() + footprint.Offset + z * slicePitch + static_cast<UINT64>(y) * rowPitch;
            const float* src = grid.values.data() + (static_cast<size_t>(z) * n + y) * n;
            std::memcpy(dst, src, static_cast<size_t>(rowBytes));
        }
    }
    mUpload = gpu::createUploadBuffer(mDevice, staging.data(), staging.size());

    // ---- 3. 写す命令と、 「ピクセルシェーダーから読む」 状態へ変える命令を記録する ----
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = mTexture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = mUpload.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;
    commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mTexture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);

    // ---- 4. SRV を作る (前のテクスチャの SRV に上書きする) ----
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = DXGI_FORMAT_R32_FLOAT;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture3D.MipLevels = 1;
    mDevice->CreateShaderResourceView(mTexture.Get(), &srv, mHeap->GetCPUDescriptorHandleForHeapStart());
}
