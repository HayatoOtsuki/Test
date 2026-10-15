// =============================================================================
// SampleCommon.hlsli — 全画面で描くサンプルのシェーダーが共通で使う物
//
//   ・エンジンのシーンの定数 (カメラ・ライト) と環境 (空・床・ToDisplay): Environment.hlsli
//   ・VSMain: 頂点の番号 (0, 1, 2) から、 画面を覆う大きな三角形を作る
//   ・CameraRay: 画素の正規化デバイス座標から、 カメラの光線 (位置と向き) を作る
//
// サンプルのシェーダーはこれを #include し、 自分の PSMain だけを書く。
// =============================================================================

#ifndef SAMPLE_COMMON_HLSLI
#define SAMPLE_COMMON_HLSLI

#include "../../../src/engine/shaders/Environment.hlsli"

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

// 画素 ndc を通るカメラの光線。 画素を奥の面 (z = 1) まで延ばした点をワールドへ戻し、 カメラからその点への向きを取る
void CameraRay(float2 ndc, out float3 ro, out float3 rd)
{
    float4 farPoint = mul(float4(ndc, 1.0, 1.0), InvViewProj);
    ro = CameraPos;
    rd = normalize(farPoint.xyz / farPoint.w - CameraPos);
}

#endif
