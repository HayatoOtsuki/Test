// =============================================================================
// IceMesh.hlsl — プリミティブ (メッシュ) の氷
//
//   VSMain: 頂点を物体の空間 → ワールド → クリップ空間へ移す
//   PSMain: 面の色を決める
//
// この段では形を確かめるため、 不透明に塗る (太陽と空で照らし、 UV の格子を描く)。
// 反射と屈折は feature/ice-optics で足す。
// =============================================================================

#include "IceCommon.hlsli"

// 頂点 (IceVertex と同じ並び)。 名前 (POSITION など) で CPU の入力レイアウトとつながる
struct VSInput
{
    float3 position : POSITION;  // 物体の空間の位置
    float3 normal   : NORMAL;    // 外向きの法線
    float2 uv       : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;  // クリップ空間の位置 (ラスタライザーが使う)
    float3 worldPos : TEXCOORD0;    // ワールドの位置 (ピクセルシェーダーで光線を作るのに使う)
    float3 normal   : TEXCOORD1;    // ワールドの法線 (平行移動だけなので物体の空間と同じ)
    float2 uv       : TEXCOORD2;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    // 氷は回さず、 位置だけずらす (物体の空間 + Position = ワールド)
    output.worldPos = input.position + Position;
    output.position = mul(float4(output.worldPos, 1.0), ViewProj);
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}

// 面の色 (表示用)。
//   ・下地は淡い水色。 UV を 8 × 8 に分けた格子の線を少し暗く描き、 UV が正しく貼れているかを見る
//   ・太陽の光 (法線と太陽の向きの cos) と、 空の光 (上を向く面ほど多い) で照らす
float3 ShadeIceMesh(float3 worldPos, float3 normal, float2 uv)
{
    float3 n = normalize(normal);  // 三角形の中で補間された法線は長さが 1 からずれるので、 直す

    float2 cell = frac(uv * 8.0);
    float gridLine = (cell.x < 0.04 || cell.y < 0.04) ? 1.0 : 0.0;
    float3 albedo = lerp(float3(0.70, 0.85, 0.95), float3(0.35, 0.50, 0.65), gridLine);

    float3 sun = SUN_COLOR * saturate(dot(n, SunDirection()));
    float3 ambient = float3(0.22, 0.30, 0.42) * (0.6 + 0.4 * n.y);
    return ToDisplay(albedo * (sun + ambient));
}

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(ShadeIceMesh(input.worldPos, input.normal, input.uv), 1.0);
}
