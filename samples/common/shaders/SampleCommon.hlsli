// =============================================================================
// SampleCommon.hlsli — サンプルのシェーダーが共通で使う物
//
//   ・エンジンのシーンの定数 (カメラ・ライト) と環境 (空・床): Environment.hlsli。 出す色は光の強さのまま (表示用の変換はエンジンが最後に 1 回だけ)
//   ・CameraRay: 画素の正規化デバイス座標から、 カメラの光線 (位置と向き) を作る
//
// 全画面で描くサンプルは、 これに全画面の頂点シェーダーを足した Fullscreen.hlsli を #include する。
// =============================================================================

#ifndef SAMPLE_COMMON_HLSLI
#define SAMPLE_COMMON_HLSLI

#include "../../../src/engine/shaders/Environment.hlsli"

// 画素 ndc を通るカメラの光線。 画素を奥の面 (z = 1) まで延ばした点をワールドへ戻し、 カメラからその点への向きを取る
void CameraRay(float2 ndc, out float3 ro, out float3 rd)
{
    float4 farPoint = mul(float4(ndc, 1.0, 1.0), InvViewProj);
    ro = CameraPos;
    rd = normalize(farPoint.xyz / farPoint.w - CameraPos);
}

#endif
