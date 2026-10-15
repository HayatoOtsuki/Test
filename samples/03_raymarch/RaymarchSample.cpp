#include "RaymarchSample.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace {

// RaymarchFog.hlsl の cbuffer RaymarchConstants と同じ並び (32 bit × 4 個)
struct RaymarchConstants {
    uint32_t stepCount;
    uint32_t shadow;
    float density;
    float pad;
};
static_assert(sizeof(RaymarchConstants) == 4 * 4, "RaymarchConstants must be four 32-bit values");

constexpr int kMinSteps = 2;
constexpr int kMaxSteps = 256;  // シェーダーの MAX_STEPS と同じ
constexpr int kShadowSteps = 8; // シェーダーの SHADOW_STEPS と同じ

} // namespace

RaymarchSample::RaymarchSample(const std::wstring& shaderDir)
    : mPass(shaderDir + L"/RaymarchFog.hlsl", sizeof(RaymarchConstants) / 4)
{
}

void RaymarchSample::initializeImplement(const RenderSetup& setup)
{
    mPass.initialize(setup);
}

bool RaymarchSample::reloadImplement(std::string& error)
{
    return mPass.reload(error);
}

void RaymarchSample::drawImplement(const DrawContext& context)
{
    RaymarchConstants constants = {};
    constants.stepCount = static_cast<uint32_t>(mStepCount);
    constants.shadow = mShadow ? 1u : 0u;
    constants.density = mDensity;
    mPass.draw(context, &constants);
}

void RaymarchSample::onKey(WPARAM key)
{
    if (key == VK_UP) mStepCount = std::min(mStepCount * 2, kMaxSteps);
    if (key == VK_DOWN) mStepCount = std::max(mStepCount / 2, kMinSteps);
    if (key == 'S') mShadow = !mShadow;
    if (key == VK_OEM_4) mDensity = std::max(mDensity - 1.0f, 0.0f);  // [
    if (key == VK_OEM_6) mDensity = std::min(mDensity + 1.0f, 30.0f); // ]
}

std::wstring RaymarchSample::getStatus() const
{
    // 1 画素あたりの密度の読み出し: 1 回目の歩数 × (1 + 影なら 2 回目の歩数)
    const int reads = mStepCount * (1 + (mShadow ? kShadowSteps : 0));
    wchar_t text[160];
    std::swprintf(text, 160, L"歩数: %d | 影: %ls | 濃さ: %.0f /m | 1 画素の密度の読み出し: %d 回", mStepCount,
                  mShadow ? L"あり" : L"なし", mDensity, reads);
    return text;
}
