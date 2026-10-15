#include "BakedSdfSample.h"

#include "BakedShape.h"
#include "SdfBaker.h"

#include <chrono>
#include <cstdint>
#include <cstdio>

namespace {

// BakedSdf.hlsl の cbuffer BakedSdfConstants と同じ並び (32 bit × 8 個)
struct BakedSdfConstants {
    Float3 boundsMin;
    float voxelSize;
    Float3 boundsMax;
    uint32_t showGrid;
};
static_assert(sizeof(BakedSdfConstants) == 8 * 4, "BakedSdfConstants must be eight 32-bit values");

const int kSizes[] = { 8, 16, 32, 64, 128 };  // キー 1〜5
constexpr int kSizeCount = 5;

} // namespace

BakedSdfSample::BakedSdfSample(const std::wstring& shaderDir)
    : mPass(shaderDir + L"/BakedSdf.hlsl", sizeof(BakedSdfConstants) / 4, true)  // true = t0 のテクスチャを使う
{
}

void BakedSdfSample::initializeImplement(const RenderSetup& setup)
{
    mPass.initialize(setup);
    mTexture.initialize(setup.device);
}

bool BakedSdfSample::reloadImplement(std::string& error)
{
    return mPass.reload(error);
}

void BakedSdfSample::bake(ID3D12GraphicsCommandList* commandList)
{
    // CPU で焼く。 かかった時間も測る (128³ = 約 210 万点)
    const auto start = std::chrono::steady_clock::now();
    const SdfGrid grid = bakeSdf(mWantedSize, baked_shape::boundsMin(), baked_shape::boundsMax(), baked_shape::sdf);
    const auto end = std::chrono::steady_clock::now();
    mBakeMilliseconds = std::chrono::duration<double, std::milli>(end - start).count();

    // GPU へ写す命令を記録する。 実行されるのは、 このフレームのコマンドリストが送られたとき (この後の描画より前)
    mTexture.upload(commandList, grid);

    // シェーダーが uvw を出すのに使う値だけ残す (距離の配列は GPU に写したので要らない)
    mBoundsMin = grid.boundsMin;
    mBoundsMax = grid.boundsMax;
    mVoxelSize = grid.voxelSize;
    mBakedSize = mWantedSize;
}

void BakedSdfSample::drawImplement(const DrawContext& context)
{
    // 細かさが変わっていたら焼き直す。 前のフレームは Renderer が待ち終えているので、 古いテクスチャを捨ててよい
    if (mBakedSize != mWantedSize) bake(context.commandList);

    BakedSdfConstants constants = {};
    constants.boundsMin = mBoundsMin;
    constants.voxelSize = mVoxelSize;
    constants.boundsMax = mBoundsMax;
    constants.showGrid = mShowGrid ? 1u : 0u;
    mPass.draw(context, &constants, mTexture.getHeap(), mTexture.getSrv());
}

void BakedSdfSample::onKey(WPARAM key)
{
    if (key >= '1' && key < '1' + kSizeCount) mWantedSize = kSizes[key - '1'];
    if (key == 'G') mShowGrid = !mShowGrid;
}

std::wstring BakedSdfSample::getStatus() const
{
    const double points = static_cast<double>(mBakedSize) * mBakedSize * mBakedSize;
    wchar_t text[200];
    std::swprintf(text, 200, L"格子: %d³ (%.0f 点, %.2f MB) | 1 マス: %.1f cm | 焼く時間: %.1f ms | 格子の線: %ls",
                  mBakedSize, points, points * 4.0 / (1024.0 * 1024.0), mVoxelSize * 100.0f, mBakeMilliseconds,
                  mShowGrid ? L"あり" : L"なし");
    return text;
}
