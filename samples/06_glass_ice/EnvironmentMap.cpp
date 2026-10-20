#include "EnvironmentMap.h"

#include "EnvironmentImage.h"

#include "common/GpuHelpers.h"
#include "engine/DxCheck.h"

#include <cstdint>
#include <cstring>
#include <vector>

void EnvironmentMap::initialize(ID3D12Device* device)
{
    mDevice = device;

    // SRV は、 シェーダーから見える (SHADER_VISIBLE) ヒープに置かないと、 描画中に読めない
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 2;  // t0: 環境マップ、 t1: 画面のコピー
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mHeap)), "CreateDescriptorHeap(environment SRV)");
}

void EnvironmentMap::setSceneCopy(ID3D12Resource* sceneCopy)
{
    if (!sceneCopy) return;
    // ヒープの 2 番目の場所 (t1)。 記述子の大きさはデバイスによって違うので、 聞いてから進める
    D3D12_CPU_DESCRIPTOR_HANDLE slot = mHeap->GetCPUDescriptorHandleForHeapStart();
    slot.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    mDevice->CreateShaderResourceView(sceneCopy, nullptr, slot);  // 記述に nullptr: リソースの形式どおりの 2D の SRV
}

void EnvironmentMap::update(ID3D12GraphicsCommandList* commandList, const Float3& lightDirection, const Float3& lightColor)
{
    const auto same = [](const Float3& a, const Float3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; };
    if (mValid && same(lightDirection, mLastDirection) && same(lightColor, mLastColor)) return;  // 変わっていない
    mValid = true;
    mLastDirection = lightDirection;
    mLastColor = lightColor;

    // ---- 1. CPU で 6 面の色を作る (背景と同じ景色の式) ----
    const std::vector<float> faces = environment_image::makeFaces(kSize, lightDirection, lightColor);

    // ---- 2. テクスチャ: 2D の配列 6 枚 (キューブマップは、 6 枚の配列を 「キューブとして読む」 SRV で見る) ----
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = kSize;
    texDesc.Height = kSize;
    texDesc.DepthOrArraySize = 6;                        // +X, −X, +Y, −Y, +Z, −Z
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;     // 光の強さのまま (太陽は 1 を超える)
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    ThrowIfFailed(mDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
                                                   D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mTexture)),
                  "CreateCommittedResource(environment map)");

    // ---- 3. 一時バッファに、 GPU の決まりの並び (行を 256 バイトの倍数) で 6 面を書く ----
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprints[6] = {};
    UINT rowCounts[6] = {};
    UINT64 rowBytes[6] = {};
    UINT64 totalBytes = 0;
    mDevice->GetCopyableFootprints(&texDesc, 0, 6, 0, footprints, rowCounts, rowBytes, &totalBytes);

    std::vector<uint8_t> staging(static_cast<size_t>(totalBytes));
    for (int face = 0; face < 6; ++face) {
        for (UINT y = 0; y < rowCounts[face]; ++y) {
            uint8_t* dst = staging.data() + footprints[face].Offset + static_cast<UINT64>(y) * footprints[face].Footprint.RowPitch;
            const float* src = faces.data() + (static_cast<size_t>(face) * kSize + y) * kSize * 4;
            std::memcpy(dst, src, static_cast<size_t>(rowBytes[face]));
        }
    }
    mUpload = gpu::createUploadBuffer(mDevice, staging.data(), staging.size());

    // ---- 4. 面ごとに写す命令と、 「ピクセルシェーダーから読む」 状態へ変える命令 ----
    for (UINT face = 0; face < 6; ++face) {
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = mTexture.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = face;  // ミップが 1 段なので、 配列の番号がそのままサブリソースの番号
        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = mUpload.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprints[face];
        commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mTexture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);

    // ---- 5. SRV: 6 枚の配列を 「キューブマップ」 として読む ----
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = texDesc.Format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.TextureCube.MipLevels = 1;
    mDevice->CreateShaderResourceView(mTexture.Get(), &srv, mHeap->GetCPUDescriptorHandleForHeapStart());
}
