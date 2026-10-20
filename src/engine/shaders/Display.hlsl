// =============================================================================
// Display.hlsl — フレームの最後に 1 回だけ、 表示用の色に直す
//
//   VSMain: 頂点の番号 (0, 1, 2) から画面を覆う三角形を作る (Background.hlsl と同じ)
//   PSMain: シーンの描画先 (明るさのまま) を同じ画素で読み、 ToDisplay (トーンマップとガンマ) をかけて出す
//
// ほかのシェーダーは表示用の変換をしない。 明るさのまま float4(色, α) を返すだけでよい。
// =============================================================================

#include "Environment.hlsli"  // ToDisplay

Texture2D<float4> SceneColor : register(t0);  // シーンの描画先 (SceneColorBuffer、 明るさのまま)

float4 VSMain(uint id : SV_VertexID) : SV_Position
{
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

float4 PSMain(float4 position : SV_Position) : SV_Target
{
    float3 color = SceneColor.Load(int3(position.xy, 0)).rgb;  // 同じ画素の明るさ (SV_Position の xy は画素の中心)
    return float4(ToDisplay(color), 1.0);                       // 表示用の色に (ここだけ)
}
