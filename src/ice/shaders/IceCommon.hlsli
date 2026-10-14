// =============================================================================
// IceCommon.hlsli — 氷のシェーダーが共通で使う物
//
//   ・カメラとライト: エンジンのシーンの定数 (b0、 SceneConstants.hlsli)。 Environment.hlsli が #include している
//   ・環境 (空・床・表示用の変換): エンジンの Environment.hlsli
//   ・氷ごとの値 (b1): IceImplement.cpp の IceObjectConstants と同じ並び
//
// 自分のエンジンへ移すときは、 Environment.hlsli の #include を、 エンジンのシーンの定数と空や背景を読む物に置き換える
// (氷のシェーダーが使うのは ViewProj / CameraPos / SunDirection / SunColor / Environment / ToDisplay)。
// =============================================================================

#include "../../engine/shaders/Environment.hlsli"

// 氷ごとの値 (ルート引数 1)。 C++ の IceObjectConstants (Material + 位置) と同じ並び。
// 16 バイト (float 4 個) ずつの区切りに合わせて並べてある
cbuffer IceObject : register(b1)
{
    float4 BaseColor;    // [0] 下地の色 (Material.baseColor、 RGBA)
    float  Metallic;     // [1] 金属らしさ (0〜1)
    float  Roughness;    //     表面の粗さ (0.04〜1)
    float  Specular;     //     金属でない面の、 正面から見た反射の強さ (0.5 で 4%)
    float  ObjectPad0;
    float3 Position;     // [2] 氷の位置 (ワールド)。 物体の空間の点にこれを足すとワールドの点になる
    float  ObjectPad1;
};
