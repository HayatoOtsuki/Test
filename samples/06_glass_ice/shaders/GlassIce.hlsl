// =============================================================================
// GlassIce.hlsl — サンプル 06: コップの氷 (短い版)
//
// 流れ: RaymarchSurface (表面を探す) → 表面 (映り込み + 屈折して中へ)
//       → RaymarchExit (出口まで進む。 吸収と白い芯) → 出口 (屈折して外へ / 全反射で中へ) → 背景
// Step (1〜7) で、 氷らしく見せる要素を 1 つずつ足す。 解説: samples/06_glass_ice/README.md
// =============================================================================

#include "../../common/shaders/Fullscreen.hlsli"

// C++ の GlassIceConstants と同じ並び
cbuffer GlassIceConstants : register(b1)
{
    uint   Step;  // 1 強い吸収 / 2 吸収を弱く / 3 屈折 / 4 全反射 / 5 白い芯 / 6 薄い影 / 7 コップ
    float3 GlassIcePad;
};

static const float IOR = 1.31;     // 氷の屈折率
static const float F0 = 0.018;     // 正面から見たときの反射率 = ((1.31 − 1) / (1.31 + 1))²
static const float EPS = 0.002;    // 表面から少し離す距離 (自分の表面にまた当たらないように)
static const float CUBE_HALF = 0.13;
static const float CUBE_ROUND = 0.035;
static const float GLASS_R = 0.36;
static const float GLASS_H = 0.8;

// ---- 形 (SDF) ----

float2 Rotate(float2 v, float a)
{
    float c = cos(a), s = sin(a);
    return float2(c * v.x - s * v.y, s * v.x + c * v.y);
}

// 角の丸い立方体。 center に置き、 y 軸まわりに yaw、 z 軸まわりに tilt だけ回す
float Cube(float3 p, float3 center, float yaw, float tilt)
{
    p -= center;
    p.xz = Rotate(p.xz, yaw);
    p.xy = Rotate(p.xy, tilt);
    float3 q = abs(p) - (CUBE_HALF - CUBE_ROUND);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - CUBE_ROUND;
}

// 氷 3 つ (下に 2 つ、 その上に傾けて 1 つ) + 溶けかけのゆるいでこぼこ (細かくすると霜に見える)
float IceSdf(float3 p)
{
    float d = min(Cube(p, float3(-0.15, 0.13, 0.06), 0.5, 0.0), Cube(p, float3(0.14, 0.13, -0.07), -0.3, 0.0));
    d = min(d, Cube(p, float3(0.0, 0.43, 0.0), 1.1, 0.4));
    return d + 0.004 * sin(p.x * 23.0) * sin(p.y * 19.0) * sin(p.z * 21.0);
}

float3 IceNormal(float3 p)
{
    const float2 e = float2(0.001, 0.0);
    return normalize(float3(IceSdf(p + e.xyy) - IceSdf(p - e.xyy), IceSdf(p + e.yxy) - IceSdf(p - e.yxy),
                            IceSdf(p + e.yyx) - IceSdf(p - e.yyx)));
}

// ---- 光 ----

// Fresnel (Schlick): 浅い角度ほど強く映り込む。 c = 光線と法線の cos
float Fresnel(float c)
{
    return F0 + (1.0 - F0) * pow(1.0 - saturate(c), 5.0);
}

// 1 m あたりの吸収 (Beer–Lambert)。 赤が多く吸われるほど青い。 小さな氷では、 ほぼ無色が正しい
float3 Absorption()
{
    return Step >= 2 ? float3(0.10, 0.04, 0.02) : float3(2.5, 0.9, 0.4);
}

// 白い芯 (冷凍庫の氷): 表面から 5 cm より深い所に、 細かい気泡が集まる (1 m あたりの濃さ)
float CoreDensity(float depth)
{
    return Step >= 5 ? 15.0 * smoothstep(0.05, 0.11, depth) : 0.0;
}

// 床の点 p から太陽へ氷をたどり、 やわらかい影を出す。 氷は光を通すので、 不透明な物より薄い影にする (0.4 まで)
float IceShadow(float3 p)
{
    float v = 1.0, t = 0.02;
    [loop]
    for (int i = 0; i < 32; ++i) {
        float d = IceSdf(p + SunDirection() * t);
        v = min(v, 8.0 * d / t);
        t += max(d, 0.01);
        if (v < 0.0 || t > 2.0) break;
    }
    return lerp(0.4, 1.0, saturate(v));
}

// 氷の外の景色 (空と床)。 Step 6 からは床に氷の影を落とす
float3 Background(float3 ro, float3 rd)
{
    if (Step < 6 || rd.y >= 0.0) return Environment(ro, rd);
    float t = max(-ro.y / rd.y, 0.0);
    float3 p = ro + rd * t;
    float s = IceShadow(p);
    return ApplyFog(ShadeFloorLit(p, float3(s, s, s)), rd, t);
}

// ---- レイマーチング ----

// 外から氷の表面を探す (球追跡)
bool RaymarchSurface(float3 ro, float3 rd, out float t)
{
    t = 0.0;
    [loop]
    for (int i = 0; i < 128; ++i) {
        float d = IceSdf(ro + rd * t);
        if (d < 0.0005) return true;
        t += d * 0.9;  // でこぼこの分、 距離が少し不正確なので控えめに
        if (t > 10.0) break;
    }
    return false;
}

// 中から出口を探す。 中では −SDF が 「いちばん近い表面までの距離」 なので、 その分進めば表面を越えない。
// 進みながら吸収 (thr を減らす) と白い芯の散乱 (col に足す) を計算する。 戻り値は出口までの距離
float RaymarchExit(float3 ro, float3 rd, inout float3 col, inout float3 thr)
{
    float t = 0.0;
    [loop]
    for (int i = 0; i < 64; ++i) {
        float depth = -IceSdf(ro + rd * t);             // 表面からの深さ
        if (depth < 0.0005) break;                      // 出口に着いた
        float s = clamp(depth, 0.0005, 0.02);           // 芯の濃さを拾えるよう、 1 歩は 2 cm まで
        float a = 1.0 - exp(-CoreDensity(depth) * s);   // この 1 歩で気泡に当たる割合
        col += thr * a * float3(0.85, 0.9, 0.95);       // 当たった光は白く散らばる
        thr *= (1.0 - a) * exp(-Absorption() * s);      // 気泡に遮られた分と、 吸収された分を減らす
        t += s;
    }
    return t;
}

// ---- コップ: 薄いガラスの筒。 壁が薄いので光の曲がりは無視して、 映り込みと少しの色だけ重ねる ----
float3 AddGlass(float3 col, float3 ro, float3 rd, float tIce)
{
    // 光線と円柱の交点: |ro.xz + rd.xz·t|² = R² を t について解く (2 次方程式)
    float a = dot(rd.xz, rd.xz);
    float b = dot(ro.xz, rd.xz);
    float h = b * b - a * (dot(ro.xz, ro.xz) - GLASS_R * GLASS_R);
    if (h < 0.0) return col;
    [loop]
    for (int side = 1; side >= -1; side -= 2) {  // 奥の壁 → 手前の壁 の順に重ねる
        float t = (-b + float(side) * sqrt(h)) / a;
        float3 p = ro + rd * t;
        if (t < 0.0 || t > tIce || p.y < 0.0 || p.y > GLASS_H) continue;  // 氷より奥の壁は、 氷に隠れる
        float3 n = normalize(float3(p.x, 0.0, p.z));
        float F = 2.0 * Fresnel(abs(dot(rd, n)));  // 壁の外側と内側の 2 面で映る
        col = col * (1.0 - F) * float3(0.96, 0.99, 0.98) + F * Background(p, reflect(rd, n));
    }
    return col;
}

// ---- 画素の色 ----
float3 ShadeGlassIce(float2 ndc)
{
    float3 ro, rd;
    CameraRay(ndc, ro, rd);
    float3 camRo = ro, camRd = rd;
    float3 col = float3(0.0, 0.0, 0.0);  // 目に届く光 (足していく)
    float3 thr = float3(1.0, 1.0, 1.0);  // まだ届く割合 (掛けて減らしていく)
    float firstHit = 1e9;

    [loop]
    for (int ice = 0; ice < 3; ++ice) {  // 隣の氷に入り直すのは 3 回まで
        float t;
        if (!RaymarchSurface(ro, rd, t)) break;
        if (ice == 0) firstHit = t;
        float3 p = ro + rd * t;
        float3 n = IceNormal(p);

        // Surface: 映り込みを足し、 残りは屈折して中へ
        float F = Fresnel(dot(-rd, n));
        col += thr * F * Background(p, reflect(rd, n));
        thr *= 1.0 - F;
        if (Step >= 3) rd = refract(rd, n, 1.0 / IOR);
        p -= n * EPS;

        // Exit: 出口まで進む。 出られない角度 (全反射) なら中で跳ね返って、 また出口を探す
        bool escaped = false;
        [loop]
        for (int k = 0; k < 4; ++k) {
            p += rd * RaymarchExit(p, rd, col, thr);
            n = IceNormal(p);
            float3 outDir = Step >= 3 ? refract(rd, -n, IOR) : rd;
            if (dot(outDir, outDir) < 1e-6) {    // 屈折の式に解が無い = 全反射
                if (Step >= 4) {
                    rd = reflect(rd, n);
                    p -= n * EPS;
                    continue;
                }
                outDir = rd;                     // Step 3: 出られないはずの光が、 そのまま出てしまう
            }
            thr *= 1.0 - Fresnel(dot(outDir, n));
            rd = outDir;
            p += n * EPS;
            escaped = true;
            break;
        }
        ro = p;
        if (!escaped) break;  // 跳ね返り続けた光は、 今の向きの景色で近似する
    }
    col += thr * Background(ro, rd);
    if (Step >= 7) col = AddGlass(col, camRo, camRd, firstHit);
    return ToDisplay(col);
}

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(ShadeGlassIce(input.ndc), 1.0);
}
