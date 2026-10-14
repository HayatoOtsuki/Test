#pragma once
// =============================================================================
// SceneConstants.h — シーンの定数 (カメラとライト)
//
// Renderer が毎フレーム 1 つの定数バッファに書き、 どのシェーダーもレジスター b0 で読む。
// HLSL 側の宣言は shaders/SceneConstants.hlsli。 並びを変えるときは両方を同じ順に直すこと。
//
// HLSL の定数は 16 バイト (float 4 個) ずつに区切られるので、
// float3 の後には float を 1 つ置いて区切りに合わせる (time, pad0, pad1)。
// =============================================================================

#include "MathTypes.h"

struct SceneConstants {
    Float4x4 viewProj;     // ワールド → クリップ空間 (転置済み)
    Float4x4 invViewProj;  // クリップ空間 → ワールド (転置済み)。 背景で画素ごとの光線を作る
    Float3 cameraPos;      // カメラの位置 (ワールド)
    float time;            // 起動からの秒
    Float3 lightDirection; // 面から太陽へ向かう向き (長さ 1)
    float pad0;
    Float3 lightColor;     // 太陽の色 × 強さ
    float pad1;
};
static_assert(sizeof(SceneConstants) == 44 * 4, "SceneConstants must match the HLSL cbuffer (44 floats)");
