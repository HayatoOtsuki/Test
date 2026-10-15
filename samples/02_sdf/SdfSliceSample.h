#pragma once
// =============================================================================
// SdfSliceSample.h — サンプル 02: SDF だけ
//
// 空間に置いた 1 枚の板の上で SDF を計算し、 距離を色と等高線で見る。 形の表面は探さない (光線は板に当てるだけ)。
//   1〜8: 形 (球・箱・角の丸い箱・トーラス・足す・削る・なめらかに足す・氷)
//   ↑ ↓: 板を動かす   P: 板の向き (縦 / 横)
// =============================================================================

#include "common/FullscreenPass.h"
#include "common/SampleEntity.h"

#include <string>

class SdfSliceSample : public SampleEntity {
public:
    explicit SdfSliceSample(const std::wstring& shaderDir);

    void onKey(WPARAM key) override;
    std::wstring getStatus() const override;

private:
    void initializeImplement(const RenderSetup& setup) override;
    void drawImplement(const DrawContext& context) override;
    bool reloadImplement(std::string& error) override;

    FullscreenPass mPass;
    int mShape = 7;         // 初めは氷の形
    float mSlice = 0.0f;    // 板の位置 (縦なら z、 横なら y)
    bool mHorizontal = false;
};
