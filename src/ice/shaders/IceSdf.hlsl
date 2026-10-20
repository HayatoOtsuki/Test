// =============================================================================
// IceSdf.hlsl — SDF (符号付き距離場) の氷
//
//   VSMain: 格子が覆う箱のメッシュを、 プリミティブと同じようにクリップ空間へ移す
//   PSMain: 箱が映る画素ごとに、 カメラからの光線で箱の中の SDF を球追跡し、 氷の表面を探す。
//           当たれば、 光線を反射・屈折させながら氷の中と外を追う。 当たらなければ捨てる (discard)
//
// 解説: docs/05-generate-sdf.md (SDF と球追跡)、 docs/06-ice-optics.md (反射・屈折・吸収・全反射)、
//       docs/07-ice-bubbles.md (気泡)
// =============================================================================

#include "IceSdfCommon.hlsli"  // SDF を読む・光線と箱・球追跡・気泡の密度 (影のシェーダーと共有)

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;    // 箱の法線 (使わない)
    float2 uv       : TEXCOORD0; // 使わない
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD0;  // 箱の面の点 (ワールド)。 カメラからこの点への向きが、 この画素の光線
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.worldPos = input.position + Position;
    output.position = mul(float4(output.worldPos, 1.0), ViewProj);
    return output;
}

// -----------------------------------------------------------------------------
// 光の追跡 (反射・屈折・吸収・全反射)
// -----------------------------------------------------------------------------

static const int MAX_SEGMENTS = 8;     // 1 本の光線で追う区間 (外・中) の数の上限
static const float SURFACE_EPS = 0.002; // 表面から少しずらす量 (2 mm)。 同じ表面にまた当たらないように

// Schlick の Fresnel の近似 (色なし)。 cosTheta: 空気の側の、 光線と法線の角度の cos
float FresnelDielectric(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - saturate(cosTheta), 5.0);
}

// 気泡に当たって散らばる光の明るさ。 中の気泡は、 周りから入ってきた光 (空と太陽) にぼんやり照らされている
float3 BubbleLight()
{
    return SunColor() * 0.25 + float3(0.35, 0.42, 0.55);
}

// 氷の中の点 ro から向き rd へ進み、 外へ出るまでの距離を返す。 途中で
//   ・気泡が散らばらせた光を col に足し (散乱)、 気泡で遮られた分 thr を減らす
//   ・進んだ長さだけ吸収する (Beer–Lambert)
// 中では SDF が負 (−φ = 表面までの距離)。 その距離ずつ進んでも表面を越えないが、 出口に近づくほど 1 歩が小さくなるので、
// 下限 (格子の半分) を付けて進み、 外 (φ > 0) に出たら、 中の点と外の点の間を 2 分探索して出口を絞る
float MarchInside(float3 ro, float3 rd, inout float3 col, inout float3 thr)
{
    float tIn = 0.0;          // 中だと分かっている t
    float t = 0.0;
    float bubbleT = 1.0;      // 気泡に遮られずに残っている割合
    [loop]
    for (int i = 0; i < 96; ++i) {
        float3 q = ro + rd * t;
        float d = SampleSdf(q);
        if (d > 0.0) break;  // 外へ出た
        // 上限 5 cm: 気泡の濃さの変わり方を拾えるくらいの細かさで進む
        float s = clamp(-d, VoxelSize * 0.5, 0.05);

        // この 1 歩 (長さ s) で気泡に当たる割合 a = 1 − exp(−密度 × s)。
        // 当たった光は散らばり、 その一部が目に届く (BubbleLight)。 ここまでの吸収 exp(−σa × t) も掛ける
        float a = 1.0 - exp(-BubbleDensityAt(q, -d) * s);
        col += thr * exp(-Absorption * t) * (bubbleT * a) * BubbleLight();
        bubbleT *= 1.0 - a;

        tIn = t;
        t += s;
    }
    // 2 分探索: tIn (中) と t (外) の真ん中が外なら外の側を、 中なら中の側を縮める。 6 回で幅は 1/64
    [loop]
    for (int j = 0; j < 6; ++j) {
        float m = 0.5 * (tIn + t);
        if (SampleSdf(ro + rd * m) > 0.0) t = m;
        else tIn = m;
    }
    // 出口までの全部の長さで吸収し、 気泡に遮られた分も減らす
    thr *= exp(-Absorption * t) * bubbleT;
    return t;
}

// この画素の色 (光の強さのまま)。 氷に当たらなければ false を返す
bool ShadeIceSdf(float3 worldPos, out float3 color)
{
    color = float3(0.0, 0.0, 0.0);

    // 光線は物体の空間で追う。 氷は回さないので、 ワールドから位置 Position を引くだけで物体の空間になる
    float3 ro = CameraPos - Position;
    float3 rd = normalize(worldPos - CameraPos);

    // 1. 最初に氷の表面に当たる所を探す (箱との交差 → 球追跡)。 当たらなければ、 この画素は描かない
    float tNear, tFar;
    if (!IntersectBox(ro, rd, tNear, tFar)) return false;
    float tHit;
    if (!SphereTrace(ro, rd, max(tNear, 0.0), tFar, tHit)) return false;

    float eta = RefractiveIndex;                          // 氷の屈折率 (空気は 1)
    float F0 = ((eta - 1.0) / (eta + 1.0)) * ((eta - 1.0) / (eta + 1.0));  // 正面からの反射率 (1.31 なら 0.018)

    float3 p = ro + rd * tHit;      // 今いる表面の点
    float3 n = SdfNormal(p);        // その外向きの法線
    float3 col = float3(0.0, 0.0, 0.0);   // 目に届く光 (足していく)
    float3 thr = float3(1.0, 1.0, 1.0);   // ここまでで残っている光の割合 (反射・吸収で減っていく)
    bool inside = false;

    // 2. 区間ごとに追う: 外 → 表面で反射と屈折 → 中 → 出口で屈折か全反射 → 外 (また当たるか) …
    [loop]
    for (int seg = 0; seg < MAX_SEGMENTS; ++seg) {
        if (!inside) {
            // ---- 表面に外から当たった: 一部は反射、 残りは屈折して中へ ----
            float F = FresnelDielectric(dot(-rd, n), F0);
            // 反射した光は、 反射の向きの景色 (空と床) をそのまま足す
            col += thr * F * Environment(p + Position + n * SURFACE_EPS, reflect(rd, n));
            thr *= 1.0 - F;
            // スネルの法則で曲げて中へ (空気 → 氷 は 1 / eta)。 中へ入るときは全反射は起きない
            rd = refract(rd, n, 1.0 / eta);
            p -= n * SURFACE_EPS;
            inside = true;
        } else {
            // ---- 中を出口まで進む。 気泡の散乱を足し、 進んだ長さだけ吸収する ----
            float len = MarchInside(p, rd, col, thr);
            p += rd * len;
            n = SdfNormal(p);

            // ---- 出口: 屈折して外へ出られるか ----
            // 氷 → 空気は eta。 浅い角度だと、 屈折の式に解が無く 0 が返る (全反射)
            float3 outDir = refract(rd, -n, eta);
            if (dot(outDir, outDir) < 1e-6) {
                // 全反射: 光は外へ出られず、 表面で鏡のように跳ね返って中を進み続ける
                rd = reflect(rd, n);
                p -= n * SURFACE_EPS;
            } else {
                // 外へ出る。 出口でも一部は中へ反射するが、 その光は追わずに捨てる (簡単のため)
                thr *= 1.0 - FresnelDielectric(dot(outDir, n), F0);
                rd = outDir;
                p += n * SURFACE_EPS;
                inside = false;

                // 外へ出た光線が、 へこんだ所でまた氷に当たるか調べる。 当たらなければ、 その先の景色で終わり
                float tN, tF, tAgain;
                if (IntersectBox(p, rd, tN, tF) && SphereTrace(p, rd, max(tN, 0.0), tF, tAgain)) {
                    p += rd * tAgain;
                    n = SdfNormal(p);
                } else {
                    col += thr * Environment(p + Position, rd);
                    thr = float3(0.0, 0.0, 0.0);
                    break;
                }
            }
        }
    }
    // 区間の上限まで出られなかった光 (全反射を繰り返した光) が残っていれば、 今の向きの景色で近似して足す。
    // 捨てると、 その画素だけ黒い点になる。 本当の光も、 何度か跳ね返った後でいずれ外へ出る
    if (inside) col += thr * Environment(p + Position, rd);

    color = col;  // 光の強さのまま (表示用の変換は最後に 1 回だけ)
    return true;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float3 color;
    if (!ShadeIceSdf(input.worldPos, color)) discard;  // 氷に当たらない画素は描かない (背景が見える)
    return float4(color, 1.0);
}
