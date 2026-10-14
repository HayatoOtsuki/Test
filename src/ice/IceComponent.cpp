// =============================================================================
// IceComponent.cpp — 窓口の中身
//
// 作るときに IceImplement を Renderer に登録し、 壊すときに外す。
// セッター・ゲッターは IceImplement の値 (IceParams) を読み書きする。
// =============================================================================

#include "IceComponent.h"

#include "IceImplement.h"

#include "engine/Renderer.h"

#include <algorithm>

IceComponent::IceComponent(Renderer& renderer, const std::wstring& shaderDir)
    : mRenderer(renderer), mImplement(std::make_unique<IceImplement>(shaderDir))
{
    // 登録すると、 renderer が mImplement の initialize (パイプラインを作る) を呼び、
    // 以後、 毎フレーム draw3D を呼ぶ
    mRenderer.addEntity(mImplement.get());
}

IceComponent::~IceComponent()
{
    // mImplement が壊れる前に、 renderer の一覧から外す (外さないと壊れた物を描こうとする)
    mRenderer.removeEntity(mImplement.get());
}

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

void IceComponent::setPosition(const Float3& position)
{
    mImplement->getParams().position = position;
}

Float3 IceComponent::getPosition() const
{
    return mImplement->getParams().position;
}

void IceComponent::setBaseColor(const Float3& color)
{
    mImplement->getParams().baseColor = { std::clamp(color.x, 0.0f, 1.0f), std::clamp(color.y, 0.0f, 1.0f),
                                          std::clamp(color.z, 0.0f, 1.0f) };
}

Float3 IceComponent::getBaseColor() const
{
    return mImplement->getParams().baseColor;
}

void IceComponent::setMetallic(float metallic)
{
    mImplement->getParams().metallic = std::clamp(metallic, 0.0f, 1.0f);
}

float IceComponent::getMetallic() const
{
    return mImplement->getParams().metallic;
}

void IceComponent::setRoughness(float roughness)
{
    mImplement->getParams().roughness = std::clamp(roughness, 0.04f, 1.0f);
}

float IceComponent::getRoughness() const
{
    return mImplement->getParams().roughness;
}

void IceComponent::setSpecular(float specular)
{
    mImplement->getParams().specular = std::clamp(specular, 0.0f, 1.0f);
}

float IceComponent::getSpecular() const
{
    return mImplement->getParams().specular;
}
