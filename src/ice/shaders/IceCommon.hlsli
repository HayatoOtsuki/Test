// =============================================================================
// IceCommon.hlsli — 氷のシェーダーが共通で使う物
//
//   ・カメラとライト: エンジンのシーンの定数 (b0、 SceneConstants.hlsli)。 Environment.hlsli が #include している
//   ・環境 (空・床・表示用の変換): エンジンの Environment.hlsli
//   ・氷ごとの値 (b1): IceImplement.cpp の IceObjectConstants と同じ並び
//   ・PBR (金属・粗さ) の面の色 ShadeStandard: プリミティブ (IceMesh.hlsl) と SDF の形 (IceSdf.hlsl) が使う
//
// 自分のエンジンへ移すときは、 Environment.hlsli の #include を、 エンジンのシーンの定数と空や背景を読む物に置き換える
// (氷のシェーダーが使うのは ViewProj / CameraPos / SunDirection / SunColor / Environment / ToDisplay)。
// =============================================================================

#ifndef ICE_COMMON_HLSLI
#define ICE_COMMON_HLSLI

#include "../../engine/shaders/Environment.hlsli"

static const float PI = 3.14159265;

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
    float3 BoundsMin;    // [3] SDF の格子が覆う箱の最小の角 (物体の空間)
    float  VoxelSize;    //     格子 1 つ分の大きさ (メートル)
    float3 BoundsMax;    // [4] 同じく最大の角
    float  ObjectPad2;
};

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
    float3 L = SunDirection();                    // 面から太陽への向き (シーンの定数から)
    float3 H = normalize(V + L);                  // V と L の真ん中の向き

    float NdotL = saturate(dot(N, L));
    float NdotV = max(dot(N, V), 1e-4);           // 0 で割らないよう、 ごく小さい値で止める
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    // 金属・粗さの流儀:
    //   金属でない面: 拡散反射の色 = BaseColor.rgb、 鏡面反射の F0 = 0.08 × Specular (無色)
    //   金属        : 拡散反射なし、 鏡面反射の F0 = BaseColor.rgb (色付きの映り込み)
    //   その間は Metallic で混ぜる
    float3 F0 = lerp(float3(0.08, 0.08, 0.08) * Specular, BaseColor.rgb, Metallic);
    float3 diffuseColor = BaseColor.rgb * (1.0 - Metallic);

    // ---- 太陽からの光 (直接光) ----
    float a = Roughness * Roughness;
    float3 F = FresnelSchlick(VdotH, F0);
    float3 specular = DistributionGGX(NdotH, a) * GeometrySmith(NdotV, NdotL, Roughness) * F
                    / (4.0 * NdotV * max(NdotL, 1e-4));
    // 拡散反射 (Lambert) は BRDF = 色 / π。 鏡面反射で跳ね返った分 (F) は中へ入らないので (1 − F) を掛ける。
    // SunColor は 「白い面を真上から照らしたときの明るさ」 として決めてあるので、 BRDF に π を掛けて使う
    float3 direct = ((1.0 - F) * diffuseColor + PI * specular) * SunColor() * NdotL;

    // ---- 周り (空と床) からの光 (間接光) ----
    // 拡散: 空のあらゆる向きからの光をまとめた一定の色 (上を向く面ほど多く受ける)。
    // 鏡面: 反射の向きの景色を 1 本だけ読む。 本当は粗いほどぼやけるが、 ここでは粗いほど弱めるだけの近似
    float3 ambientDiffuse = float3(0.22, 0.30, 0.42) * (0.6 + 0.4 * N.y) * diffuseColor;
    float3 R = reflect(-V, N);
    float3 envF = FresnelSchlick(NdotV, F0);
    float3 ambientSpecular = Environment(worldPos, R) * envF * (1.0 - Roughness) * (1.0 - Roughness);

    return ToDisplay(direct + ambientDiffuse + ambientSpecular);
}

#endif
