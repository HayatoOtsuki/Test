// =============================================================================
// SceneConstants.hlsli — シーンの定数 (カメラとライト) の HLSL 側
//
// エンジン (Renderer) が毎フレーム 1 つの定数バッファに書き、 ルート引数でレジスター b0 に結び付ける。
// どのシェーダー (背景・プリミティブ・氷) も、 このファイルを #include するだけで同じ値を読める。
// C++ 側の宣言は src/engine/SceneConstants.h。 並びは両方で同じにすること。
// =============================================================================

#ifndef SCENE_CONSTANTS_HLSLI
#define SCENE_CONSTANTS_HLSLI

cbuffer SceneConstants : register(b0)
{
    float4x4 ViewProj;        // ワールド → クリップ空間 (CPU で転置済み。 mul(float4(p, 1), ViewProj) と書く)
    float4x4 InvViewProj;     // クリップ空間 → ワールド (同じく転置済み)
    float3   CameraPos;       // カメラの位置 (ワールド)
    float    Time;            // 起動からの秒
    float3   LightDirection;  // 面から太陽へ向かう向き (長さ 1)
    float    ScenePad0;
    float3   LightColor;      // 太陽の色 × 強さ
    float    ScenePad1;
};

#endif
