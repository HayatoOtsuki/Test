// =============================================================================
// Primitive.hlsl — サンプル 01: プリミティブ (メッシュ) を描く
//
//   VSMain: 頂点をワールド → クリップ空間へ移す (ViewProj はエンジンのシーンの定数から)
//   PSMain: 面の色。 N キーで見方を変える: 光 (ランバート + ハイライト) / 法線 / UV
// =============================================================================

#include "../../common/shaders/SampleCommon.hlsli"

// C++ の PrimitiveConstants と同じ並び
cbuffer PrimitiveConstants : register(b1)
{
    float3 BaseColor;  // 面の色
    uint   ColorMode;  // 0 = 光、 1 = 法線、 2 = UV
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;  // 三角形の中では、 3 つの頂点の法線がなめらかに混ぜられて届く
    float2 uv       : TEXCOORD2;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.worldPos = input.position;  // このサンプルでは物は動かさない (物体の空間 = ワールド)
    output.position = mul(float4(output.worldPos, 1.0), ViewProj);
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}

// 面の色 (表示用)
float3 ShadePrimitive(float3 worldPos, float3 normal, float2 uv)
{
    float3 n = normalize(normal);  // 混ぜた法線は長さが 1 からずれるので直す

    // 法線を色で: x, y, z (−1〜1) を r, g, b (0〜1) に。 上を向く面は緑、 +x は赤、 +z は青っぽい
    if (ColorMode == 1) return n * 0.5 + 0.5;
    // UV を色で: u を赤、 v を緑。 模様の貼り方 (継ぎ目・向き) が分かる
    if (ColorMode == 2) return float3(frac(uv), 0.0);

    // 光: ランバート (面が太陽を向くほど明るい) + 空の光 + ブリン・フォンのハイライト
    float3 L = SunDirection();
    float3 V = normalize(CameraPos - worldPos);
    float3 H = normalize(L + V);
    float3 diffuse = BaseColor * (SunColor() * saturate(dot(n, L)) + float3(0.22, 0.30, 0.42) * (0.6 + 0.4 * n.y));
    float3 highlight = SunColor() * 0.25 * pow(saturate(dot(n, H)), 64.0);
    return ToDisplay(diffuse + highlight);
}

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(ShadePrimitive(input.worldPos, input.normal, input.uv), 1.0);
}
