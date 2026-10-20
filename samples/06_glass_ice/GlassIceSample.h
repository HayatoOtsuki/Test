#pragma once
// =============================================================================
// GlassIceSample.h — サンプル 06: コップの氷
//
// 氷 3 つとコップ 1 つを、 それぞれ SdfObject (部品 1 つ = 形 1 つ) として置く。 このクラス自身は何も描かない。
// 描く順: (このクラスが環境マップを更新) → 氷 → コップ (コップは半透明で、 後ろに描いた氷に重ねるので最後)
// シェーダーは段階ごとのファイル IceShader0.hlsl 〜 IceShader10.hlsl。 0〜9 と ← → で切り替える (コップは段階 10 だけ)
// =============================================================================

#include "EnvironmentMap.h"
#include "SdfObject.h"

#include "common/SampleEntity.h"

#include <string>
#include <vector>

class GlassIceSample : public SampleEntity {
public:
    explicit GlassIceSample(const std::wstring& shaderDir);

    void onKey(WPARAM key) override;
    std::wstring getStatus() const override;
    std::vector<RenderEntity*> getParts() override;

private:
    void initializeImplement(const RenderSetup& setup) override;
    void drawImplement(const DrawContext& context) override;
    bool reloadImplement(std::string& error) override;

    void setStage(int stage);  // 全部の部品のシェーダーを、 段階 stage のファイルにする

    std::wstring mShaderDir;
    EnvironmentMap mEnvironment;  // 周りの景色のキューブマップ (全部の部品が t0 で読む)
    int mStage = 10;  // 完成形から始める
    SdfObject mIce0;
    SdfObject mIce1;
    SdfObject mIce2;
    SdfObject mCup;
};
