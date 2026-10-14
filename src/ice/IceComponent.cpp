// =============================================================================
// IceComponent.cpp — 窓口の中身
//
// セッター・ゲッターは IceImplement の値 (IceParams) を読み書きする。
// Renderer から呼ばれる 3 つの関数は、 IceImplement へ渡すだけ (位置は RenderEntity から渡す)。
// =============================================================================

#include "IceComponent.h"

#include "IceImplement.h"

#include <algorithm>

namespace {

// マテリアルの値を、 シェーダーで困らない範囲に収める
Material clampMaterial(const Material& m)
{
    Material r = m;
    r.baseColor = { std::clamp(m.baseColor.x, 0.0f, 1.0f), std::clamp(m.baseColor.y, 0.0f, 1.0f),
                    std::clamp(m.baseColor.z, 0.0f, 1.0f), std::clamp(m.baseColor.w, 0.0f, 1.0f) };
    r.metallic = std::clamp(m.metallic, 0.0f, 1.0f);
    r.roughness = std::clamp(m.roughness, 0.04f, 1.0f);  // 0 だと GGX の分布が 1 点に集まり、 ハイライトが消える
    r.specular = std::clamp(m.specular, 0.0f, 1.0f);
    return r;
}

} // namespace

IceComponent::IceComponent(const std::wstring& shaderDir) : mImplement(std::make_unique<IceImplement>(shaderDir))
{
}

IceComponent::~IceComponent() = default;

// ---- RenderEntity ----

void IceComponent::initializeImplement(const RenderSetup& setup)
{
    mImplement->initialize(setup);
}

void IceComponent::drawImplement(const DrawContext& context)
{
    mImplement->draw3D(context, getPosition());  // 位置は RenderEntity が持っている
}

bool IceComponent::reloadImplement(std::string& error)
{
    return mImplement->reloadShaders(error);
}

// ---- 形 ----

void IceComponent::setShape(IceShape shape)
{
    // mGeom の添え字になるので、 範囲の外の値は受け付けない
    if (static_cast<int>(shape) < 0 || shape >= IceShape::Count) return;
    mImplement->getParams().shape = shape;
}

IceShape IceComponent::getShape() const
{
    return mImplement->getParams().shape;
}

// ---- マテリアル ----

void IceComponent::setMaterial(const Material& material)
{
    mImplement->getParams().material = clampMaterial(material);
}

const Material& IceComponent::getMaterial() const
{
    return mImplement->getParams().material;
}

void IceComponent::setBaseColor(const Float4& color)
{
    Material m = getMaterial();
    m.baseColor = color;
    setMaterial(m);
}

const Float4& IceComponent::getBaseColor() const
{
    return getMaterial().baseColor;
}

void IceComponent::setMetallic(float metallic)
{
    Material m = getMaterial();
    m.metallic = metallic;
    setMaterial(m);
}

float IceComponent::getMetallic() const
{
    return getMaterial().metallic;
}

void IceComponent::setRoughness(float roughness)
{
    Material m = getMaterial();
    m.roughness = roughness;
    setMaterial(m);
}

float IceComponent::getRoughness() const
{
    return getMaterial().roughness;
}

void IceComponent::setSpecular(float specular)
{
    Material m = getMaterial();
    m.specular = specular;
    setMaterial(m);
}

float IceComponent::getSpecular() const
{
    return getMaterial().specular;
}

// ---- SDF の氷の値 ----

void IceComponent::setRefractiveIndex(float n)
{
    // 1 より小さいと 「氷の方が空気より光が速い」 ことになり、 屈折の向きが逆になる。 上はダイヤモンド (2.42) くらいまで
    mImplement->getParams().refractiveIndex = std::clamp(n, 1.0f, 2.5f);
}

float IceComponent::getRefractiveIndex() const
{
    return mImplement->getParams().refractiveIndex;
}

void IceComponent::setAbsorptionColor(const Float3& color)
{
    // 0 だと吸収の係数 −ln(色) が無限大になるので、 0.01 で止める
    mImplement->getParams().absorptionColor = { std::clamp(color.x, 0.01f, 1.0f), std::clamp(color.y, 0.01f, 1.0f),
                                                std::clamp(color.z, 0.01f, 1.0f) };
}

const Float3& IceComponent::getAbsorptionColor() const
{
    return mImplement->getParams().absorptionColor;
}
