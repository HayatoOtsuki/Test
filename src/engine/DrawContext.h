#pragma once
// =============================================================================
// DrawContext.h — エンジンから部品の draw3D へ渡す物
//
// 部品 (氷など) はこれだけを受け取って描く。 自分のエンジンへ移すときは、
// エンジンの同じ役目の物 (コマンドリスト・カメラの行列と位置) に置き換える。
// =============================================================================

#include <DirectXMath.h>
#include <d3d12.h>

struct DrawContext {
    // 記録中のコマンドリスト。 描画先 (バックバッファと深度)・ビューポート・シザーは設定済み
    ID3D12GraphicsCommandList* commandList = nullptr;

    // ワールド → クリップ空間の行列 (ビュー × 射影)。
    // HLSL の定数は列優先で読まれるので、 転置して入れてある。 HLSL では mul(float4(p, 1), ViewProj) と書く
    DirectX::XMFLOAT4X4 viewProj = {};

    DirectX::XMFLOAT3 cameraPos = {};  // カメラの位置 (ワールド、 メートル)
    float time = 0.0f;                 // 起動からの秒
};
