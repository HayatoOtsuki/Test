#pragma once
// =============================================================================
// IceComponent.h — 氷の部品の窓口
//
// エンジンの RenderEntity (画面に描く物の基底クラス) を継承する。 そのため
//   ・位置 (setPosition / getPosition) は RenderEntity から自動で持つ
//   ・Renderer::addEntity で登録すると、 Renderer が毎フレーム描く
//
// このクラスが外 (ゲームの側) へ見せるのは、 形とマテリアルのセッター・ゲッターだけ。
// 描く仕組み (initialize / draw3D / reloadShaders) の中身は、 private の mImplement (IceImplement) が持つ。
// Renderer から呼ばれる RenderEntity の仮想関数は private で override し、 mImplement へ渡すだけ。
// =============================================================================

#include "engine/Material.h"
#include "engine/MathTypes.h"
#include "engine/RenderEntity.h"

#include <memory>
#include <string>

class IceImplement;

// 氷の形の種類。 mGeom (形の配列) の添え字にもなる
enum class IceShape {
    Cube,      // プリミティブ: 立方体
    Sphere,    // プリミティブ: 球
    Cylinder,  // プリミティブ: 円柱
    Count      // 形の数 (いつも最後に置く。 for で全部の形を回すのに使う)
};

class IceComponent : public RenderEntity {
public:
    // shaderDir: 氷のシェーダーのフォルダ (src/ice/shaders)
    explicit IceComponent(const std::wstring& shaderDir);
    ~IceComponent() override;  // IceImplement の中身が要るので .cpp に書く

    IceComponent(const IceComponent&) = delete;
    IceComponent& operator=(const IceComponent&) = delete;

    // ---- セッター・ゲッター (位置は RenderEntity の setPosition / getPosition) ----

    // 形。 範囲の外 (Count 以上) は無視する
    void setShape(IceShape shape);
    IceShape getShape() const;

    // マテリアル (プリミティブの面の質感) をまとめて変える / 読む。 値は範囲に収める
    void setMaterial(const Material& material);
    const Material& getMaterial() const;

    // マテリアルの値を 1 つずつ変える / 読む
    void setBaseColor(const Float4& color);  // 各 0〜1
    const Float4& getBaseColor() const;
    void setMetallic(float metallic);  // 0〜1
    float getMetallic() const;
    void setRoughness(float roughness);  // 0.04〜1 (0 だとハイライトが点になって消えるため)
    float getRoughness() const;
    void setSpecular(float specular);  // 0〜1。 0.5 で 4%
    float getSpecular() const;

private:
    // ---- RenderEntity: Renderer だけが呼ぶ。 中身は IceImplement に任せる ----
    void initializeImplement(const RenderSetup& setup) override;
    void drawImplement(const DrawContext& context) override;
    bool reloadImplement(std::string& error) override;

    std::unique_ptr<IceImplement> mImplement;  // 描く仕組みの全部
};
