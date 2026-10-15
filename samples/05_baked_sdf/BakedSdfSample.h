#pragma once
// =============================================================================
// BakedSdfSample.h — サンプル 05: SDF を焼いて描く
//
// SDF の式を CPU で格子に焼き (SdfBaker)、 3D テクスチャにして (SdfTexture)、 シェーダーではテクスチャを読んで球追跡する。
//   1〜5: 格子の細かさ (8³ / 16³ / 32³ / 64³ / 128³)   G: 格子の線を表面に描く
// =============================================================================

#include "SdfTexture.h"

#include "common/FullscreenPass.h"
#include "common/SampleEntity.h"

#include <string>

class BakedSdfSample : public SampleEntity {
public:
    explicit BakedSdfSample(const std::wstring& shaderDir);

    void onKey(WPARAM key) override;
    std::wstring getStatus() const override;

private:
    void initializeImplement(const RenderSetup& setup) override;
    void drawImplement(const DrawContext& context) override;
    bool reloadImplement(std::string& error) override;

    // CPU で焼いて、 テクスチャへ写す命令を commandList に記録する
    void bake(ID3D12GraphicsCommandList* commandList);

    FullscreenPass mPass;
    SdfTexture mTexture;
    int mWantedSize = 64;  // キーで選んだ細かさ
    int mBakedSize = 0;    // 今のテクスチャの細かさ (0 = まだ焼いていない)
    double mBakeMilliseconds = 0.0;
    Float3 mBoundsMin;
    Float3 mBoundsMax;
    float mVoxelSize = 0.0f;
    bool mShowGrid = false;
};
