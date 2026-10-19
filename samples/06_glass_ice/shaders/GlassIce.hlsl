// =============================================================================
// GlassIce.hlsl — サンプル 06: コップの氷 (短い版)
//
// 関数に分けているのは、 ノイズと SDF (形・法線) だけ。 描き方は全部 PSMain の中に上から順に書いてある:
//   1. 外から氷の表面を探す (球追跡)
//   2. 表面: 映り込み + 屈折して中へ
//   3. 中を出口まで進む (吸収・白い芯) → 出口: 屈折して外へ / 全反射で中へ。 後ろの面の映り込み
//   4. 出た向きの景色 (床なら氷の薄い影)
//   5. コップ (薄いガラスの映り込み)
// Step (1〜7) で、 氷らしく見せる要素を 1 つずつ足す。 解説: samples/06_glass_ice/README.md
// =============================================================================

#include "../../common/shaders/Fullscreen.hlsli"

// C++ の GlassIceConstants と同じ並び
cbuffer GlassIceConstants : register(b1)
{
    uint   Step;  // 1 強い吸収 / 2 吸収を弱く / 3 屈折 / 4 全反射と後ろの面 / 5 白い芯 / 6 薄い影 / 7 コップ
    float3 GlassIcePad;
};

static const float IOR = 1.31;     // 氷の屈折率
static const float F0 = 0.018;     // 氷を正面から見たときの反射率 = ((1.31 − 1) / (1.31 + 1))²
static const float EPS = 0.002;    // 表面から少し離す距離 (自分の表面にまた当たらないように)
static const float CUBE_HALF = 0.13;
static const float CUBE_ROUND = 0.035;
static const float GLASS_R = 0.36;
static const float GLASS_H = 0.8;

// ---- ノイズ ----

float Hash(float3 p)
{
    return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453);
}

// 値ノイズ (−1〜1): 格子点の乱数をなめらかに混ぜた、 ゆっくり変わる値
float Noise(float3 p)
{
    float3 i = floor(p);
    float3 f = p - i;
    float3 u = f * f * (3.0 - 2.0 * f);
    float x00 = lerp(Hash(i), Hash(i + float3(1.0, 0.0, 0.0)), u.x);
    float x10 = lerp(Hash(i + float3(0.0, 1.0, 0.0)), Hash(i + float3(1.0, 1.0, 0.0)), u.x);
    float x01 = lerp(Hash(i + float3(0.0, 0.0, 1.0)), Hash(i + float3(1.0, 0.0, 1.0)), u.x);
    float x11 = lerp(Hash(i + float3(0.0, 1.0, 1.0)), Hash(i + float3(1.0, 1.0, 1.0)), u.x);
    return lerp(lerp(x00, x10, u.y), lerp(x01, x11, u.y), u.z) * 2.0 - 1.0;
}

// ---- SDF (形) ----

// 角の丸い立方体。 center に置き、 y 軸まわりに yaw、 z 軸まわりに tilt だけ回す
float Cube(float3 p, float3 center, float yaw, float tilt)
{
    p -= center;
    p.xz = float2(cos(yaw) * p.x - sin(yaw) * p.z, sin(yaw) * p.x + cos(yaw) * p.z);
    p.xy = float2(cos(tilt) * p.x - sin(tilt) * p.y, sin(tilt) * p.x + cos(tilt) * p.y);
    float3 q = abs(p) - (CUBE_HALF - CUBE_ROUND);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - CUBE_ROUND;
}

// 氷 3 つ (下に 2 つ、 その上に傾けて 1 つ) + 溶けかけのゆるいでこぼこ (ノイズを細かくすると霜に見える)
float IceSdf(float3 p)
{
    float d = min(Cube(p, float3(-0.15, 0.13, 0.06), 0.5, 0.0), Cube(p, float3(0.14, 0.13, -0.07), -0.3, 0.0));
    d = min(d, Cube(p, float3(0.0, 0.43, 0.0), 1.1, 0.4));
    return d + 0.005 * Noise(p * 8.0);
}

// 法線 = SDF の傾き (中心差分)
float3 IceNormal(float3 p)
{
    const float2 e = float2(0.001, 0.0);
    return normalize(float3(IceSdf(p + e.xyy) - IceSdf(p - e.xyy), IceSdf(p + e.yxy) - IceSdf(p - e.yxy),
                            IceSdf(p + e.yyx) - IceSdf(p - e.yyx)));
}

// ---- ピクセルシェーダー: 描き方は全部ここ ----
float4 PSMain(VSOutput input) : SV_Target
{
    float3 ro, rd;
    CameraRay(input.ndc, ro, rd);
    float3 camRo = ro, camRd = rd;
    float3 col = float3(0.0, 0.0, 0.0);  // 目に届く光 (足していく)
    float3 thr = float3(1.0, 1.0, 1.0);  // まだ届く割合 (掛けて減らしていく)
    float firstHit = 1e9;                // 最初に氷に当たった距離 (コップの奥の壁との前後に使う)
    // 1 m あたりの吸収 (Beer–Lambert)。 赤が多く吸われるほど青い。 小さな氷では、 ほぼ無色が正しい
    float3 absorb = Step >= 2 ? float3(0.10, 0.04, 0.02) : float3(2.5, 0.9, 0.4);

    [loop]
    for (int ice = 0; ice < 3; ++ice) {  // 氷から出た光が、 隣の氷にまた入るのは 3 回まで
        // ---- 1. 外から表面を探す (球追跡: SDF の値だけ進めば、 表面を越えない) ----
        float t = 0.0;
        bool hit = false;
        [loop]
        for (int i = 0; i < 128; ++i) {
            float d = IceSdf(ro + rd * t);
            if (d < 0.0005) {
                hit = true;
                break;
            }
            t += d * 0.9;  // でこぼこの分、 距離が少し不正確なので控えめに
            if (t > 10.0) break;
        }
        if (!hit) break;
        if (ice == 0) firstHit = t;
        float3 p = ro + rd * t;
        float3 n = IceNormal(p);

        // ---- 2. 表面: 映り込み (Fresnel) を足し、 残りは屈折して中へ ----
        float F = F0 + (1.0 - F0) * pow(1.0 - saturate(dot(-rd, n)), 5.0);  // 浅い角度ほど強く映る
        col += thr * F * Environment(p, reflect(rd, n));
        thr *= 1.0 - F;
        if (Step >= 3) rd = refract(rd, n, 1.0 / IOR);  // 空気 → 氷
        p -= n * EPS;

        // ---- 3. 中を出口まで進む。 出られない角度 (全反射) なら跳ね返って、 また出口を探す ----
        bool escaped = false;
        [loop]
        for (int k = 0; k < 4; ++k) {
            // 3a. 中では −SDF が 「いちばん近い表面までの距離」。 その分進めば表面を越えない (1 歩は 2 cm まで)。
            //     進みながら、 吸収 exp(−σ × 歩幅) を掛け、 芯の気泡に当たった分を白く足す
            [loop]
            for (int j = 0; j < 64; ++j) {
                float depth = -IceSdf(p);  // 表面からの深さ
                if (depth < 0.0005) break;  // 出口 (後ろの面) に着いた
                float s = clamp(depth, 0.0005, 0.02);
                float core = Step >= 5 ? 15.0 * smoothstep(0.05, 0.11, depth) : 0.0;  // 白い芯の濃さ (1/m)
                float a = 1.0 - exp(-core * s);  // この 1 歩で気泡に当たる割合
                col += thr * a * float3(0.85, 0.9, 0.95);
                thr *= (1.0 - a) * exp(-absorb * s);
                p += rd * s;
            }

            // 3b. 出口 (後ろの面): 氷 → 空気。 屈折の式に解が無ければ全反射
            n = IceNormal(p);
            float3 outDir = Step >= 3 ? refract(rd, -n, IOR) : rd;
            if (dot(outDir, outDir) < 1e-6) {
                if (Step >= 4) {
                    rd = reflect(rd, n);  // 中で跳ね返って、 もう一度 3a へ
                    p -= n * EPS;
                    continue;
                }
                outDir = rd;  // Step 3: 出られないはずの光が、 そのまま出てしまう
            }
            F = F0 + (1.0 - F0) * pow(1.0 - saturate(dot(outDir, n)), 5.0);
            // 後ろの面の映り込み: 中へ跳ね返る分 F を、 跳ね返った向きの景色で近似して足す
            if (Step >= 4) col += thr * F * Environment(p, reflect(rd, n));
            thr *= 1.0 - F;
            rd = outDir;  // 残りは曲がって外へ
            p += n * EPS;
            escaped = true;
            break;
        }
        ro = p;
        if (!escaped) break;  // 跳ね返り続けた光は、 今の向きの景色で近似する
    }

    // ---- 4. 最後に出た向きの景色。 床なら、 太陽へ氷をたどって薄い影を落とす ----
    float3 background = Environment(ro, rd);
    if (Step >= 6 && rd.y < 0.0) {
        float tFloor = max(-ro.y / rd.y, 0.0);
        float3 fp = ro + rd * tFloor;
        float v = 1.0, ts = 0.02;
        [loop]
        for (int m = 0; m < 32; ++m) {
            float d = IceSdf(fp + SunDirection() * ts);
            v = min(v, 8.0 * d / ts);  // 光線が氷の近くを通るほど暗い (やわらかい影)
            ts += max(d, 0.01);
            if (v < 0.0 || ts > 2.0) break;
        }
        float shadow = lerp(0.4, 1.0, saturate(v));  // 氷は光を通すので、 不透明な物より薄い影
        background = ApplyFog(ShadeFloorLit(fp, float3(shadow, shadow, shadow)), rd, tFloor);
    }
    col += thr * background;

    // ---- 5. コップ: 薄いガラスの筒。 壁が薄いので曲がりは無視し、 映り込みと少しの色だけ重ねる ----
    if (Step >= 7) {
        // 光線と円柱の交点: |ro.xz + rd.xz·t|² = R² を t について解く (2 次方程式)
        float a = dot(camRd.xz, camRd.xz);
        float b = dot(camRo.xz, camRd.xz);
        float h = b * b - a * (dot(camRo.xz, camRo.xz) - GLASS_R * GLASS_R);
        if (h >= 0.0) {
            [loop]
            for (int side = 1; side >= -1; side -= 2) {  // 奥の壁 → 手前の壁 の順に重ねる
                float tg = (-b + float(side) * sqrt(h)) / a;
                float3 gp = camRo + camRd * tg;
                if (tg < 0.0 || tg > firstHit || gp.y < 0.0 || gp.y > GLASS_H) continue;  // 氷より奥の壁は隠れる
                float3 gn = normalize(float3(gp.x, 0.0, gp.z));
                float gF = 2.0 * (0.04 + 0.96 * pow(1.0 - abs(dot(camRd, gn)), 5.0));  // ガラス (1.5) の 2 面
                col = col * (1.0 - gF) * float3(0.96, 0.99, 0.98) + gF * Environment(gp, reflect(camRd, gn));
            }
        }
    }
    return float4(ToDisplay(col), 1.0);
}
