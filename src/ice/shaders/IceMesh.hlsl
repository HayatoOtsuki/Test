// =============================================================================
// IceMesh.hlsl — プリミティブ (メッシュ) の面
//
//   VSMain: 頂点を物体の空間 → ワールド → クリップ空間へ移す
//   PSMain: 面の色を、 金属・粗さの流儀の PBR (物理ベースのシェーディング) で決める
//
// マテリアルの値 (BaseColor / Metallic / Roughness / Specular) は IceComponent のセッターで変えられる。
// 解説: docs/03-ice-component.md 「面の色 (PBR)」
// =============================================================================

#include "IceCommon.hlsli"

static const float PI = 3.14159265;

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

// -----------------------------------------------------------------------------
// PBR の部品 (Cook–Torrance のマイクロファセットモデル)
//
// 表面を、 向きがばらばらな小さな鏡 (マイクロファセット) の集まりと考える。
// 光が目に向かって鏡のように跳ね返るのは、 向きがちょうど H (光の向きと目の向きの真ん中) の鏡だけ。
//   D: 向きが H の鏡がどれだけあるか (分布)       — 粗いほど広く散らばる
//   G: 鏡どうしで隠し合って届かない分 (遮蔽)      — 浅い角度ほど効く
//   F: 鏡 1 枚がどれだけ反射するか (Fresnel)      — 浅い角度ほど強い
// 鏡面反射 = D × G × F / (4 × (N・L) × (N・V))
// -----------------------------------------------------------------------------

// D: GGX (Trowbridge–Reitz) の分布。 a は 粗さ² (見た目の粗さが均等に変わるよう 2 乗する)
float DistributionGGX(float NdotH, float a)
{
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// G: Smith の遮蔽を Schlick の形で近似したもの (光の側と目の側の 2 回分を掛ける)。
// k = (粗さ + 1)² / 8 は、 直接の光 (太陽) に合わせた値
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k);
    float gl = NdotL / (NdotL * (1.0 - k) + k);
    return gv * gl;
}

// F: Schlick の Fresnel の近似。 F0 は正面 (cos = 1) から見たときの反射率。
// 浅い角度 (cos → 0) で 1 (全部反射) に近づく
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - saturate(cosTheta), 5.0);
}

// 面の色 (表示用)
float3 ShadeStandard(float3 worldPos, float3 normal)
{
    float3 N = normalize(normal);                 // 補間で長さが 1 からずれるので直す
    float3 V = normalize(CameraPos - worldPos);   // 面から目への向き
    float3 L = SunDirection();                    // 面から太陽への向き
    float3 H = normalize(V + L);                  // V と L の真ん中の向き

    float NdotL = saturate(dot(N, L));
    float NdotV = max(dot(N, V), 1e-4);           // 0 で割らないよう、 ごく小さい値で止める
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    // 金属・粗さの流儀:
    //   金属でない面: 拡散反射の色 = BaseColor、 鏡面反射の F0 = 0.08 × Specular (無色)
    //   金属        : 拡散反射なし、 鏡面反射の F0 = BaseColor (色付きの映り込み)
    //   その間は Metallic で混ぜる
    float3 F0 = lerp(float3(0.08, 0.08, 0.08) * Specular, BaseColor, Metallic);
    float3 diffuseColor = BaseColor * (1.0 - Metallic);

    // ---- 太陽からの光 (直接光) ----
    float a = Roughness * Roughness;
    float3 F = FresnelSchlick(VdotH, F0);
    float3 specular = DistributionGGX(NdotH, a) * GeometrySmith(NdotV, NdotL, Roughness) * F
                    / (4.0 * NdotV * max(NdotL, 1e-4));
    // 拡散反射 (Lambert) は BRDF = 色 / π。 鏡面反射で跳ね返った分 (F) は中へ入らないので (1 − F) を掛ける。
    // SUN_COLOR は 「白い面を真上から照らしたときの明るさ」 として決めてあるので、 BRDF に π を掛けて使う
    float3 direct = ((1.0 - F) * diffuseColor + PI * specular) * SUN_COLOR * NdotL;

    // ---- 周り (空と床) からの光 (間接光) ----
    // 拡散: 空のあらゆる向きからの光をまとめた一定の色 (上を向く面ほど多く受ける)。
    // 鏡面: 反射の向きの景色を 1 本だけ読む。 本当は粗いほどぼやけるが、 ここでは粗いほど弱めるだけの近似
    float3 ambientDiffuse = float3(0.22, 0.30, 0.42) * (0.6 + 0.4 * N.y) * diffuseColor;
    float3 R = reflect(-V, N);
    float3 envF = FresnelSchlick(NdotV, F0);
    float3 ambientSpecular = Environment(worldPos, R) * envF * (1.0 - Roughness) * (1.0 - Roughness);

    return ToDisplay(direct + ambientDiffuse + ambientSpecular);
}

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(ShadeStandard(input.worldPos, input.normal), 1.0);
}
