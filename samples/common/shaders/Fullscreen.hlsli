// =============================================================================
// Fullscreen.hlsli — 全画面で描くサンプルのシェーダーが共通で使う物
//
//   ・SampleCommon.hlsli (シーンの定数・環境・CameraRay)
//   ・VSMain: 頂点の番号 (0, 1, 2) から、 画面を覆う大きな三角形を作る
//
// 全画面のサンプル (FullscreenPass で描く物) のシェーダーはこれを #include し、 自分の PSMain だけを書く。
// =============================================================================

#ifndef FULLSCREEN_HLSLI
#define FULLSCREEN_HLSLI

#include "SampleCommon.hlsli"

struct VSOutput
{
    float4 position : SV_Position;
    float2 ndc      : TEXCOORD0;  // 正規化デバイス座標 (画面の左下が (-1, -1)、 右上が (1, 1))
};

// 頂点 0, 1, 2 が (-1, 1), (3, 1), (-1, -3) になり、 画面 (-1〜1 の四角) をすっぽり覆う
VSOutput VSMain(uint id : SV_VertexID)
{
    float2 uv = float2((id << 1) & 2, id & 2);
    VSOutput output;
    output.ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.position = float4(output.ndc, 0.0, 1.0);
    return output;
}

#endif
