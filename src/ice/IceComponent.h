#pragma once
// =============================================================================
// IceComponent.h — 氷の部品の窓口
//
// 外 (エンジン) から使うのはこのクラスだけ。
//   ・initialize : 描く準備 (パイプラインを作る)
//   ・draw3D     : 毎フレーム呼ぶ。 最初の 1 回で全部の形を作る
//   ・セッター・ゲッター: 形・位置など、 氷の値を変える / 読む
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
    Cube,    // プリミティブ: 立方体
    Sphere,  // プリミティブ: 球
    Count    // 形の数 (いつも最後に置く。 for で全部の形を回すのに使う)
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

private:
    std::unique_ptr<IceImplement> mImplement;
};
