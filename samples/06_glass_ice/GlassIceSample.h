#pragma once
// =============================================================================
// GlassIceSample.h — サンプル 06: コップの氷
//
// 氷 3 つとコップ 1 つを、 それぞれ SdfObject (部品 1 つ = 形 1 つ) として置く。 このクラス自身は何も描かない。
// 描く順: 氷 → コップ (コップは半透明で、 後ろに描いた氷に重ねるので最後)
// =============================================================================

#include "SdfObject.h"

#include "common/SampleEntity.h"

#include <string>
#include <vector>

class GlassIceSample : public SampleEntity {
public:
    explicit GlassIceSample(const std::wstring& shaderDir);

    std::wstring getStatus() const override;
    std::vector<RenderEntity*> getParts() override;

private:
    void initializeImplement(const RenderSetup& setup) override;
    void drawImplement(const DrawContext& context) override;
    bool reloadImplement(std::string& error) override;

    SdfObject mIce0;
    SdfObject mIce1;
    SdfObject mIce2;
    SdfObject mCup;
};
