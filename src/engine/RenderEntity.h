#pragma once
// =============================================================================
// RenderEntity.h — 画面に描く物の基底クラス
//
// 描く物 (氷の IceComponent など) はこれを継承する。 継承すると自動で
//   ・位置 (setPosition / getPosition) を持つ
//   ・Renderer に登録 (Renderer::addEntity) できる
//
// 登録すると Renderer が、 次の private の仮想関数を呼ぶ:
//   ・initializeImplement : 登録のときに 1 回 (パイプラインなどを作る)
//   ・drawImplement       : 毎フレーム、 背景を描いた後
//   ・reloadImplement     : R キーでシェーダーを読み直すとき
// これらは private なので、 継承した物を使う側 (ゲームの側) からは見えない。 呼べるのは Renderer (friend) だけ。
// 継承した物は、 private のまま override して中身を書く。
// =============================================================================

#include "DrawContext.h"
#include "MathTypes.h"

#include <d3d12.h>

#include <string>

// 描く準備に要る物。 Renderer::addEntity が initializeImplement に渡す
struct RenderSetup {
    ID3D12Device* device = nullptr;
    DXGI_FORMAT colorFormat = DXGI_FORMAT_UNKNOWN;  // 描画先の色の形式 (PSO に書く)
    DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;  // 深度バッファの形式 (PSO に書く)
};

class RenderEntity {
public:
    virtual ~RenderEntity() = default;

    // 位置 (ワールド、 メートル)
    void setPosition(const Float3& position) { mPosition = position; }
    const Float3& getPosition() const { return mPosition; }

private:
    friend class Renderer;  // 下の仮想関数を呼べるのは Renderer だけ

    // 登録されたときに 1 回呼ばれる。 ルートシグネチャやパイプラインを作る
    virtual void initializeImplement(const RenderSetup& setup) = 0;

    // 毎フレーム、 背景の後に呼ばれる。 context のコマンドリストに描く命令を記録する
    virtual void drawImplement(const DrawContext& context) = 0;

    // シェーダーを読み直す。 失敗したら false と理由を返し、 前のシェーダーで描き続ける
    virtual bool reloadImplement(std::string& error) = 0;

    Float3 mPosition = { 0.0f, 0.0f, 0.0f };
};
