#pragma once
// =============================================================================
// RaymarchSample.h — サンプル 03: レイマーチングだけ
//
// 霧の玉 (密度の場) を、 光線を一定の歩幅で進めて描く。 SDF は使わない。
//   ↑ ↓: 歩数 (2 倍・半分)   S: 太陽への 2 回目のレイマーチ (影)   [ ]: 霧の濃さ
// =============================================================================

#include "common/FullscreenPass.h"
#include "common/SampleEntity.h"

#include <string>

class RaymarchSample : public SampleEntity {
public:
    explicit RaymarchSample(const std::wstring& shaderDir);

    void onKey(WPARAM key) override;
    std::wstring getStatus() const override;

private:
    void initializeImplement(const RenderSetup& setup) override;
    void drawImplement(const DrawContext& context) override;
    bool reloadImplement(std::string& error) override;

    FullscreenPass mPass;
    int mStepCount = 32;
    bool mShadow = true;
    float mDensity = 4.0f;
};
