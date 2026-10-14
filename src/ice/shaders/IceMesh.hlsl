// =============================================================================
// IceMesh.hlsl — プリミティブ (メッシュ) の面
//
//   VSMain: 頂点を物体の空間 → ワールド → クリップ空間へ移す
//   PSMain: 面の色を、 金属・粗さの流儀の PBR (物理ベースのシェーディング) で決める (IceCommon.hlsli の ShadeStandard)
//
// マテリアルの値 (BaseColor / Metallic / Roughness / Specular) は IceComponent のセッターで変えられる。
// 解説: docs/03-ice-component.md 「面の色 (PBR)」
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
    float3 worldPos : TEXCOORD0;    // ワールドの位置 (ピクセルシェーダーで目への向きを作るのに使う)
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

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(ShadeStandard(input.worldPos, input.normal), 1.0);
}
