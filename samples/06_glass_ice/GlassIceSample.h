#pragma once
// =============================================================================
// GlassIceSample.h — サンプル 06: コップの氷 (短い版)
//
// 氷を 「氷塊」 ではなく 「コップに入れた氷」 に見せる要素を、 1〜7 のキーで 1 段ずつ足して比べる。
// =============================================================================

#include "common/FullscreenPass.h"
#include "common/SampleEntity.h"

#include <string>

class GlassIceSample : public SampleEntity {
public:
    explicit GlassIceSample(const std::wstring& shaderDir);

    void onKey(WPARAM key) override;
    std::wstring getStatus() const override;

private:
    void initializeImplement(const RenderSetup& setup) override;
    void drawImplement(const DrawContext& context) override;
    bool reloadImplement(std::string& error) override;

    FullscreenPass mPass;
    int mStep = 7;  // 全部入りから始める
};
