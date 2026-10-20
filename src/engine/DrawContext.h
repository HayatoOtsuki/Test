#pragma once
// =============================================================================
// DrawContext.h — エンジンから部品の draw3D へ渡す物
//
// 部品 (氷など) はこれだけを受け取って描く。 自分のエンジンへ移すときは、
// エンジンの同じ役目の物 (コマンドリスト・カメラとライトの定数バッファ) に置き換える。
// =============================================================================

#include "SceneConstants.h"

#include <d3d12.h>

struct DrawContext {
    // 記録中のコマンドリスト。 描画先 (シーンの色と深度)・ビューポート・シザーは設定済み
    ID3D12GraphicsCommandList* commandList = nullptr;

    // シーンの定数 (カメラとライト) が入った定数バッファの GPU のアドレス。
    // 部品は自分のルートシグネチャの b0 (定数バッファビュー) にこれを渡すだけで、 シェーダーから
    // ViewProj・CameraPos・LightDirection などが読める (shaders/SceneConstants.hlsli)
    D3D12_GPU_VIRTUAL_ADDRESS sceneConstants = 0;

    // 同じ値の CPU 側の写し (C++ でカメラの位置などを使いたいとき用)
    SceneConstants scene = {};

    // 背景を描いた直後の画面のコピー (光の強さのまま、 シェーダーから読む状態)。
    // 透ける物 (氷) が、 屈折した先の周りの景色を読むのに使う。 自分のエンジンでは、 不透明な物を描き終えた後の画面のコピーに当たる
    ID3D12Resource* sceneCopy = nullptr;
};
