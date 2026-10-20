#include "GlassIceSample.h"

namespace {

constexpr int kStageCount = 14;  // IceShader0 〜 IceShader13
const wchar_t* const kStageNames[kStageCount] = { L"箱を描く", L"形を探す (球追跡)", L"法線と光 (不透明)",
                                                  L"透明にする (映り込みと透過)", L"屈折 (歪み)", L"全反射と後ろの面",
                                                  L"吸収と溶けかけの形", L"白い芯 (内部散乱の簡単版)", L"気泡", L"亀裂",
                                                  L"体積の散乱 (層 × ノイズ)", L"散乱の影 (太陽への光)",
                                                  L"前方散乱", L"周りの景色 (画面のコピー、 完成)" };

} // namespace

GlassIceSample::GlassIceSample(const std::wstring& shaderDir)
    : mShaderDir(shaderDir)
    , mIce0(shaderDir + L"/IceShader13.hlsl")
    , mIce1(shaderDir + L"/IceShader13.hlsl")
    , mIce2(shaderDir + L"/IceShader13.hlsl")
{
    // 氷: 屈折率 1.31、 吸収はごく弱く (小さな氷はほぼ無色)、 白い芯あり。 床 (y = 0) の上に 2 つ並べ、 その上に 1 つ載せる (氷の高さ 26 cm)
    SdfObject* ices[] = { &mIce0, &mIce1, &mIce2 };
    const Float3 positions[] = { { -0.15f, 0.13f, 0.06f }, { 0.14f, 0.13f, -0.07f }, { 0.0f, 0.43f, 0.0f } };
    const float yaws[] = { 0.5f, -0.3f, 1.1f };
    const float tilts[] = { 0.0f, 0.0f, 0.4f };
    for (int i = 0; i < 3; ++i) {
        ices[i]->setPosition(positions[i]);
        ices[i]->setRotation(yaws[i], tilts[i]);
        ices[i]->setRefractiveIndex(1.31f);
        ices[i]->setAbsorption({ 0.10f, 0.04f, 0.02f });
        ices[i]->setCoreDensity(15.0f);
        ices[i]->setEnvironmentMap(&mEnvironment);  // 全部の氷が、 同じ環境マップ (周りの景色) を読む
    }
}

std::vector<RenderEntity*> GlassIceSample::getParts()
{
    return { &mIce0, &mIce1, &mIce2 };
}

void GlassIceSample::setStage(int stage)
{
    mStage = stage;
    const std::wstring file = mShaderDir + L"/IceShader" + std::to_wstring(stage) + L".hlsl";
    for (SdfObject* part : { &mIce0, &mIce1, &mIce2 }) part->setShaderFile(file);
}

void GlassIceSample::onKey(WPARAM key)
{
    if (key >= '0' && key <= '9') setStage(static_cast<int>(key - '0'));
    if (key == VK_LEFT && mStage > 0) setStage(mStage - 1);
    if (key == VK_RIGHT && mStage < kStageCount - 1) setStage(mStage + 1);
}

std::wstring GlassIceSample::getStatus() const
{
    return L"IceShader" + std::to_wstring(mStage) + L".hlsl: " + kStageNames[mStage] + L" (0〜9 と ← → で段階を切り替え)";
}

// このクラス自身は描かない (部品が描く)。 部品より先に呼ばれるので、 ここで環境マップを用意する
void GlassIceSample::initializeImplement(const RenderSetup& setup)
{
    mEnvironment.initialize(setup.device);
}

void GlassIceSample::drawImplement(const DrawContext& context)
{
    // 太陽の向きか色が変わっていたら、 環境マップを作り直す (最初のフレームも)
    mEnvironment.update(context.commandList, context.scene.lightDirection, context.scene.lightColor);
    mEnvironment.setSceneCopy(context.sceneCopy);  // 背景を描いた直後の画面のコピー (段階 13 で、 屈折した先の周りの景色を読む)
}

bool GlassIceSample::reloadImplement(std::string& error)
{
    static_cast<void>(error);
    return true;
}
