#pragma once
// =============================================================================
// RenderEntity.h — Renderer が毎フレーム描く物の基底クラス
//
// 描く物 (氷の IceImplement など) はこれを継承し、 Renderer::addEntity で登録する。
// 登録すると Renderer が
//   ・登録のときに 1 回 initialize を呼ぶ (パイプラインなどを作る)
//   ・毎フレーム、 背景を描いた後に draw3D を呼ぶ
//   ・R キーで reloadShaders を呼ぶ
// 部品の窓口 (IceComponent など) は、 これらを外へ見せなくてよい。
// =============================================================================

#include "DrawContext.h"

#include <d3d12.h>

#include <string>

// 描く準備に要る物。 Renderer::addEntity が initialize に渡す
struct RenderSetup {
    ID3D12Device* device = nullptr;
    DXGI_FORMAT colorFormat = DXGI_FORMAT_UNKNOWN;  // 描画先の色の形式 (PSO に書く)
    DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;  // 深度バッファの形式 (PSO に書く)
};

class RenderEntity {
public:
    virtual ~RenderEntity() = default;

    // 登録されたときに 1 回呼ばれる。 ルートシグネチャやパイプラインを作る
    virtual void initialize(const RenderSetup& setup) = 0;

    // 毎フレーム、 背景の後に呼ばれる。 context のコマンドリストに描く命令を記録する
    virtual void draw3D(const DrawContext& context) = 0;

    // シェーダーを読み直す。 失敗したら false と理由を返し、 前のシェーダーで描き続ける
    virtual bool reloadShaders(std::string& error) = 0;
};
