#include "GlassIceSample.h"

#include <cstdint>

namespace {

// GlassIce.hlsl の cbuffer GlassIceConstants と同じ並び (32 bit × 4 個)
struct GlassIceConstants {
    uint32_t step;
    float pad[3];
};
static_assert(sizeof(GlassIceConstants) == 4 * 4, "GlassIceConstants must be four 32-bit values");

constexpr int kStepCount = 7;
const wchar_t* const kStepNames[] = { L"強い吸収 (氷塊の色)", L"吸収を弱く (小さな氷はほぼ無色)", L"屈折 (歪み)",
                                      L"全反射", L"白い芯", L"薄い影", L"コップ" };

} // namespace

GlassIceSample::GlassIceSample(const std::wstring& shaderDir)
    : mPass(shaderDir + L"/GlassIce.hlsl", sizeof(GlassIceConstants) / 4)
{
}

void GlassIceSample::initializeImplement(const RenderSetup& setup)
{
    mPass.initialize(setup);
}

bool GlassIceSample::reloadImplement(std::string& error)
{
    return mPass.reload(error);
}

void GlassIceSample::drawImplement(const DrawContext& context)
{
    GlassIceConstants constants = {};
    constants.step = static_cast<uint32_t>(mStep);
    mPass.draw(context, &constants);
}

void GlassIceSample::onKey(WPARAM key)
{
    if (key >= '1' && key < '1' + kStepCount) mStep = static_cast<int>(key - '0');
}

std::wstring GlassIceSample::getStatus() const
{
    return L"段階 " + std::to_wstring(mStep) + L" / 7: " + kStepNames[mStep - 1] + L" (1〜7 で切り替え)";
}
