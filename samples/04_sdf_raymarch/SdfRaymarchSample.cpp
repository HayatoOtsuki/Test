#include "SdfRaymarchSample.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

// SdfRaymarch.hlsl の cbuffer SdfRaymarchConstants と同じ並び (32 bit × 4 個)
struct SdfRaymarchConstants {
    uint32_t shape;
    uint32_t viewMode;
    uint32_t shadowMode;
    float stepScale;
};
static_assert(sizeof(SdfRaymarchConstants) == 4 * 4, "SdfRaymarchConstants must be four 32-bit values");

// 02 と同じ形 (どちらも SdfShapes.hlsli の SceneSdf)
const wchar_t* const kShapeNames[] = { L"球", L"箱", L"角の丸い箱", L"トーラス", L"足す (箱 + 球)",
                                       L"削る (箱 − 球)", L"なめらかに足す (球 + 球)", L"氷 (本体と同じ式)" };
constexpr int kShapeCount = 8;
const wchar_t* const kViewNames[] = { L"光", L"歩数 (青 = 少ない、 赤 = 64 歩以上)", L"法線" };
const wchar_t* const kShadowNames[] = { L"なし", L"くっきり", L"やわらか" };

constexpr float kStepScaleMin = 0.3f;
constexpr float kStepScaleMax = 2.0f;
constexpr float kStepScaleDelta = 0.1f;

} // namespace

SdfRaymarchSample::SdfRaymarchSample(const std::wstring& shaderDir)
    : mPass(shaderDir + L"/SdfRaymarch.hlsl", sizeof(SdfRaymarchConstants) / 4)
{
}

void SdfRaymarchSample::initializeImplement(const RenderSetup& setup)
{
    mPass.initialize(setup);
}

bool SdfRaymarchSample::reloadImplement(std::string& error)
{
    return mPass.reload(error);
}

void SdfRaymarchSample::drawImplement(const DrawContext& context)
{
    SdfRaymarchConstants constants = {};
    constants.shape = static_cast<uint32_t>(mShape);
    constants.viewMode = static_cast<uint32_t>(mViewMode);
    constants.shadowMode = static_cast<uint32_t>(mShadowMode);
    constants.stepScale = mStepScale;
    mPass.draw(context, &constants);
}

void SdfRaymarchSample::onKey(WPARAM key)
{
    if (key >= '1' && key < '1' + kShapeCount) mShape = static_cast<int>(key - '1');
    if (key == 'H') mViewMode = (mViewMode + 1) % 3;
    if (key == 'S') mShadowMode = (mShadowMode + 1) % 3;
    // 0.1 ずつ足すと誤差がたまるので、 0.1 の倍数に丸め直す
    if (key == VK_UP) mStepScale = std::round(std::min(mStepScale + kStepScaleDelta, kStepScaleMax) * 10.0f) / 10.0f;
    if (key == VK_DOWN) mStepScale = std::round(std::max(mStepScale - kStepScaleDelta, kStepScaleMin) * 10.0f) / 10.0f;
}

std::wstring SdfRaymarchSample::getStatus() const
{
    wchar_t scale[32];
    std::swprintf(scale, 32, L"歩幅の倍率 k = %.1f", mStepScale);
    return std::wstring(L"形: ") + kShapeNames[mShape] + L" | 表示: " + kViewNames[mViewMode] + L" | 影: " +
           kShadowNames[mShadowMode] + L" | " + scale;
}
