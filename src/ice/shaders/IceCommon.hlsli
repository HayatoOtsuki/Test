// =============================================================================
// IceCommon.hlsli — 氷のシェーダーが共通で使う物
//
//   ・CPU から届く定数 (IceImplement.cpp の IceFrameConstants / IceObjectConstants と同じ並び)
//   ・環境 (空・床・表示用の変換) はエンジンの Environment.hlsli から読む
//
// 自分のエンジンへ移すときは、 Environment.hlsli の #include を、
// エンジンの空や背景を読む関数に置き換える (氷のシェーダーが使うのは Environment / SunDirection / SUN_COLOR / ToDisplay)。
// =============================================================================

#include "../../engine/shaders/Environment.hlsli"

// フレームごとの値 (ルート引数 0)
cbuffer IceFrame : register(b0)
{
    float4x4 ViewProj;   // ワールド → クリップ空間 (CPU で転置済み。 mul(float4(p, 1), ViewProj) と書く)
    float3   CameraPos;  // カメラの位置 (ワールド)
    float    Time;       // 起動からの秒
};

// 氷ごとの値 (ルート引数 1)。 16 バイト (float 4 個) ずつの区切りに合わせて並べてある
cbuffer IceObject : register(b1)
{
    float3 Position;   // 氷の位置 (ワールド)。 物体の空間の点にこれを足すとワールドの点になる
    float  Metallic;   // 金属らしさ (0〜1)
    float3 BaseColor;  // 下地の色
    float  Roughness;  // 表面の粗さ (0.04〜1)
    float  Specular;   // 金属でない面の、 正面から見た反射の強さ (0.5 で 4%)
    float3 ObjectPad;
};
