#include "GlassIceSample.h"

GlassIceSample::GlassIceSample(const std::wstring& shaderDir)
    : mIce0(shaderDir + L"/GlassIce.hlsl", SdfObject::Shape::IceCube)
    , mIce1(shaderDir + L"/GlassIce.hlsl", SdfObject::Shape::IceCube)
    , mIce2(shaderDir + L"/GlassIce.hlsl", SdfObject::Shape::IceCube)
    , mCup(shaderDir + L"/GlassIce.hlsl", SdfObject::Shape::Cup)
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

std::wstring GlassIceSample::getStatus() const
{
    return L"氷 3 つ + コップ 1 つ (部品 1 つ = 形 1 つ)";
}

// このクラス自身は描かない (部品が描く)
void GlassIceSample::initializeImplement(const RenderSetup& setup)
{
    static_cast<void>(setup);
}

void GlassIceSample::drawImplement(const DrawContext& context)
{
    static_cast<void>(context);
}

bool GlassIceSample::reloadImplement(std::string& error)
{
    static_cast<void>(error);
    return true;
}
