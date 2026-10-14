#pragma once
// =============================================================================
// IceComponent.h — 氷の部品の窓口
//
// 外 (ゲームやエンジンの側) から使うのはこのクラスだけ。 持つのは
//   ・セッター・ゲッター: 形・位置・マテリアルなど、 氷の値を変える / 読む
//   ・private の mImplement (IceImplement): 描く仕組み (initialize / draw3D / reloadShaders) の全部
//
// 作られたときに mImplement を Renderer に登録する。 以後、 Renderer が毎フレーム mImplement の draw3D を呼ぶので、
// このクラスは描画の関数を外へ見せない。 読む側は IceImplement の中身 (DirectX 12 の細かい物) を知らなくてよい。
// =============================================================================

#include "engine/MathTypes.h"

#include <memory>
#include <string>

class IceImplement;
class Renderer;

// 氷の形の種類。 mGeom (形の配列) の添え字にもなる
enum class IceShape {
    Cube,      // プリミティブ: 立方体
    Sphere,    // プリミティブ: 球
    Cylinder,  // プリミティブ: 円柱
    Count      // 形の数 (いつも最後に置く。 for で全部の形を回すのに使う)
};

class IceComponent {
public:
    // 氷を作り、 描画 (mImplement) を renderer に登録する。 以後、 renderer が毎フレーム描く。
    //   shaderDir: 氷のシェーダーのフォルダ (src/ice/shaders)
    IceComponent(Renderer& renderer, const std::wstring& shaderDir);
    ~IceComponent();  // renderer から登録を外す。 IceImplement の中身が要るので .cpp に書く

    IceComponent(const IceComponent&) = delete;
    IceComponent& operator=(const IceComponent&) = delete;

    // ---- セッター・ゲッター ----

    // 形。 範囲の外 (Count 以上) は無視する
    void setShape(IceShape shape);
    IceShape getShape() const;

    // 位置 (ワールド、 メートル)。 氷の原点は底の真ん中なので、 y = 0 にすると床に載る
    void setPosition(const Float3& position);
    Float3 getPosition() const;

    // ---- マテリアル (プリミティブの面の質感。 金属・粗さの流儀の PBR) ----

    // 下地の色 (0〜1)。 金属でなければ拡散反射の色、 金属なら映り込みの色になる
    void setBaseColor(const Float3& color);
    Float3 getBaseColor() const;

    // 金属らしさ (0 = 金属でない、 1 = 金属)。 0〜1 に収める
    void setMetallic(float metallic);
    float getMetallic() const;

    // 表面の粗さ (0 = 鏡のようにつるつる、 1 = ざらざら)。 0.04〜1 に収める (0 だとハイライトが点になって消えるため)
    void setRoughness(float roughness);
    float getRoughness() const;

    // 金属でない面の、 正面から見た反射の強さ (0〜1)。 0.5 で 4% (多くの物の値)。 F0 = 0.08 × specular
    void setSpecular(float specular);
    float getSpecular() const;

private:
    Renderer& mRenderer;                       // 登録した先 (外すときに使う)
    std::unique_ptr<IceImplement> mImplement;  // 描く仕組みの全部
};
