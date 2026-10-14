#pragma once
// =============================================================================
// IceComponent.h — 氷の部品の窓口
//
// 外 (エンジン) から使うのはこのクラスだけ。
//   ・initialize : 描く準備 (パイプラインを作る)
//   ・draw3D     : 毎フレーム呼ぶ。 最初の 1 回で全部の形を作る
//   ・セッター・ゲッター: 形・位置・マテリアルなど、 氷の値を変える / 読む
//
// 実装は IceImplement に任せ、 private の mImplement に持つ。
// こうすると、 このヘッダーを読む側 (エンジン) は IceImplement の中身 (DirectX 12 の細かい物) を知らなくてよい。
// =============================================================================

#include "engine/DrawContext.h"
#include "engine/MathTypes.h"

#include <d3d12.h>

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

class IceComponent {
public:
    IceComponent();
    ~IceComponent();  // IceImplement の中身が要るので .cpp に書く

    IceComponent(const IceComponent&) = delete;
    IceComponent& operator=(const IceComponent&) = delete;

    // 描く準備。 ルートシグネチャとパイプラインを作る。
    //   colorFormat / depthFormat: 描画先 (バックバッファと深度バッファ) の形式
    //   shaderDir                : 氷のシェーダーのフォルダ (src/ice/shaders)
    void initialize(ID3D12Device* device, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat,
                    const std::wstring& shaderDir);

    // 毎フレーム呼ぶ。 今の形 (getShape) を描く
    void draw3D(const DrawContext& context);

    // シェーダーを読み直す。 失敗したら false と理由を返し、 前のシェーダーで描き続ける
    bool reloadShaders(std::string& error);

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
    std::unique_ptr<IceImplement> mImplement;
};
