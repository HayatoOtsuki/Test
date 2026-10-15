#pragma once
// =============================================================================
// SdfRaymarchSample.h — サンプル 04: SDF × レイマーチング
//
// 02 の SDF の形を、 03 のレイマーチングの歩幅を 「SDF の値」 にして (球追跡) 描く。
//   1〜8: 形   H: 表示 (光 / 歩数 / 法線)   S: 影 (なし / くっきり / やわらか)   ↑ ↓: 歩幅の倍率 k
// =============================================================================

#include "common/FullscreenPass.h"
#include "common/SampleEntity.h"

#include <string>

class SdfRaymarchSample : public SampleEntity {
public:
    explicit SdfRaymarchSample(const std::wstring& shaderDir);

    void onKey(WPARAM key) override;
    std::wstring getStatus() const override;

private:
    void initializeImplement(const RenderSetup& setup) override;
    void drawImplement(const DrawContext& context) override;
    bool reloadImplement(std::string& error) override;

    FullscreenPass mPass;
    int mShape = 7;          // 氷から始める
    int mViewMode = 0;       // 0 = 光、 1 = 歩数、 2 = 法線
    int mShadowMode = 2;     // 0 = なし、 1 = くっきり、 2 = やわらか
    float mStepScale = 1.0f; // 歩幅の倍率 k
};
