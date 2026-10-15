#include "SdfSliceSample.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace {

// SdfSlice.hlsl の cbuffer SdfSliceConstants と同じ並び (32 bit × 4 個)
struct SdfSliceConstants {
    uint32_t shape;
    float slice;
    uint32_t horizontal;
    float pad;
};
static_assert(sizeof(SdfSliceConstants) == 4 * 4, "SdfSliceConstants must be four 32-bit values");

const wchar_t* const kShapeNames[] = { L"球", L"箱", L"角の丸い箱", L"トーラス", L"足す (箱 + 球)",
                                       L"削る (箱 − 球)", L"なめらかに足す (球 + 球)", L"氷 (本体と同じ式)" };
constexpr int kShapeCount = 8;
constexpr float kSliceStep = 0.05f;  // ↑ ↓ で動かす幅 (メートル)

} // namespace

SdfSliceSample::SdfSliceSample(const std::wstring& shaderDir)
    : mPass(shaderDir + L"/SdfSlice.hlsl", sizeof(SdfSliceConstants) / 4)
{
}

void SdfSliceSample::initializeImplement(const RenderSetup& setup)
{
    mPass.initialize(setup);
}

bool SdfSliceSample::reloadImplement(std::string& error)
{
    return mPass.reload(error);
}

void SdfSliceSample::drawImplement(const DrawContext& context)
{
    SdfSliceConstants constants = {};
    constants.shape = static_cast<uint32_t>(mShape);
    // 横の板は、 形の真ん中の高さ (0.45 m) から動かす
    constants.slice = mHorizontal ? 0.45f + mSlice : mSlice;
    constants.horizontal = mHorizontal ? 1u : 0u;
    mPass.draw(context, &constants);
}

void SdfSliceSample::onKey(WPARAM key)
{
    if (key >= '1' && key < '1' + kShapeCount) mShape = static_cast<int>(key - '1');
    if (key == VK_UP) mSlice = std::min(mSlice + kSliceStep, 0.6f);
    if (key == VK_DOWN) mSlice = std::max(mSlice - kSliceStep, -0.6f);
    if (key == 'P') {
        mHorizontal = !mHorizontal;
        mSlice = 0.0f;
    }
}

std::wstring SdfSliceSample::getStatus() const
{
    wchar_t slice[64];
    std::swprintf(slice, 64, L"%ls = %+.2f m", mHorizontal ? L"横の板 y" : L"縦の板 z",
                  mHorizontal ? 0.45f + mSlice : mSlice);
    return std::wstring(L"形: ") + kShapeNames[mShape] + L" | " + slice;
}
