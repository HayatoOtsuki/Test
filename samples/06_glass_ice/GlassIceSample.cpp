#include "GlassIceSample.h"

namespace {

constexpr int kStageCount = 11;  // IceShader0 〜 IceShader10
const wchar_t* const kStageNames[kStageCount] = { L"箱を描く", L"形を探す (球追跡)", L"法線と光 (不透明)",
                                                  L"透明にする (映り込みと透過)", L"屈折 (歪み)", L"全反射と後ろの面",
                                                  L"吸収と溶けかけの形", L"白い芯 (内部散乱)", L"気泡", L"亀裂",
                                                  L"コップ (完成)" };

} // namespace

GlassIceSample::GlassIceSample(const std::wstring& shaderDir)
    : mShaderDir(shaderDir)
    , mIce0(shaderDir + L"/IceShader10.hlsl", SdfObject::Shape::IceCube)
    , mIce1(shaderDir + L"/IceShader10.hlsl", SdfObject::Shape::IceCube)
    , mIce2(shaderDir + L"/IceShader10.hlsl", SdfObject::Shape::IceCube)
    , mCup(shaderDir + L"/IceShader10.hlsl", SdfObject::Shape::Cup)
{
    // 氷: 屈折率 1.31、 吸収はごく弱く (小さな氷はほぼ無色)、 白い芯あり。 コップの底 (厚さ 1.2 cm) の上に積む
    SdfObject* ices[] = { &mIce0, &mIce1, &mIce2 };
    const Float3 positions[] = { { -0.15f, 0.145f, 0.06f }, { 0.14f, 0.145f, -0.07f }, { 0.0f, 0.445f, 0.0f } };
    const float yaws[] = { 0.5f, -0.3f, 1.1f };
    const float tilts[] = { 0.0f, 0.0f, 0.4f };
    for (int i = 0; i < 3; ++i) {
        ices[i]->setPosition(positions[i]);
        ices[i]->setRotation(yaws[i], tilts[i]);
        ices[i]->setRefractiveIndex(1.31f);
        ices[i]->setAbsorption({ 0.10f, 0.04f, 0.02f });
        ices[i]->setCoreDensity(15.0f);
    }

    // 全部の部品が、 同じ環境マップ (周りの景色) を読む
    for (SdfObject* part : { &mIce0, &mIce1, &mIce2, &mCup }) part->setEnvironmentMap(&mEnvironment);

    // コップ: ガラス (屈折率 1.5)、 ほんの少し緑。 底が床 (y = 0) に着く高さ (CUP_H + CUP_T)
    mCup.setPosition({ 0.0f, 0.406f, 0.0f });
    mCup.setRefractiveIndex(1.5f);
    mCup.setAbsorption({ 0.6f, 0.2f, 0.4f });
    mCup.setCoreDensity(0.0f);
}

std::vector<RenderEntity*> GlassIceSample::getParts()
{
    return { &mIce0, &mIce1, &mIce2, &mCup };
}

void GlassIceSample::setStage(int stage)
{
    mStage = stage;
    const std::wstring file = mShaderDir + L"/IceShader" + std::to_wstring(stage) + L".hlsl";
    for (SdfObject* part : { &mIce0, &mIce1, &mIce2, &mCup }) part->setShaderFile(file);
    mCup.setVisible(stage == kStageCount - 1);  // コップは段階 10 (完成) だけ
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
}

bool GlassIceSample::reloadImplement(std::string& error)
{
    static_cast<void>(error);
    return true;
}
