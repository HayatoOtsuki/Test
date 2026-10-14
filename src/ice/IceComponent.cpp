// =============================================================================
// IceComponent.cpp — 窓口の中身
//
// どの関数も IceImplement へ渡すだけ。 セッター・ゲッターは IceImplement の値 (IceParams) を読み書きする。
// =============================================================================

#include "IceComponent.h"

#include "IceImplement.h"

IceComponent::IceComponent() : mImplement(std::make_unique<IceImplement>())
{
}

IceComponent::~IceComponent() = default;

void IceComponent::initialize(ID3D12Device* device, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat,
                              const std::wstring& shaderDir)
{
    mImplement->initialize(device, colorFormat, depthFormat, shaderDir);
}

void IceComponent::draw3D(const DrawContext& context)
{
    mImplement->draw3D(context);
}

bool IceComponent::reloadShaders(std::string& error)
{
    return mImplement->reloadShaders(error);
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
