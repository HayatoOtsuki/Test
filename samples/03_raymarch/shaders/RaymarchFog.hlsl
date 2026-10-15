// =============================================================================
// RaymarchFog.hlsl — サンプル 03: レイマーチングだけ (霧の玉)
//
// 光線を 「一定の歩幅」 で少しずつ進め、 その場の霧の濃さ (密度) を読んで、 光の吸収と散乱を足していく。
// SDF は使わない (霧には表面が無いので、 「表面までの距離」 も無い)。
//   ・歩数 (StepCount) が少ないと、 層 (歩幅の跡) が見える。 多いとなめらかだが重い
//   ・影 (Shadow) を入れると、 各点からさらに太陽へ 2 回目のレイマーチをして、 太陽の反対側が暗くなる
// 解説: samples/03_raymarch/README.md
// =============================================================================

#include "../../common/shaders/Fullscreen.hlsli"

// C++ の RaymarchConstants と同じ並び
cbuffer RaymarchConstants : register(b1)
{
    uint  StepCount;  // 1 本の光線で霧の中を何歩で進むか
    uint  Shadow;     // 1 なら太陽への 2 回目のレイマーチで影を付ける
    float Density;    // 霧の濃さ (1/m)
    float FogPad;
};

static const float3 FOG_CENTER = float3(0.0, 0.5, 0.0);
static const float FOG_RADIUS = 0.5;
static const int MAX_STEPS = 256;
static const int SHADOW_STEPS = 8;

// ---- 霧の濃さ (密度の場) ----

float Hash3(float3 i)
{
    return frac(sin(dot(i, float3(127.1, 311.7, 74.7))) * 43758.5453);
}

// 値ノイズ (−1〜1): 格子点の乱数をなめらかに補間した、 ゆっくり変わる値
float ValueNoise(float3 p)
{
    float3 i = floor(p);
    float3 f = p - i;
    float3 u = f * f * (3.0 - 2.0 * f);
    float x00 = lerp(Hash3(i), Hash3(i + float3(1.0, 0.0, 0.0)), u.x);
    float x10 = lerp(Hash3(i + float3(0.0, 1.0, 0.0)), Hash3(i + float3(1.0, 1.0, 0.0)), u.x);
    float x01 = lerp(Hash3(i + float3(0.0, 0.0, 1.0)), Hash3(i + float3(1.0, 0.0, 1.0)), u.x);
    float x11 = lerp(Hash3(i + float3(0.0, 1.0, 1.0)), Hash3(i + float3(1.0, 1.0, 1.0)), u.x);
    return lerp(lerp(x00, x10, u.y), lerp(x01, x11, u.y), u.z) * 2.0 - 1.0;
}

// 点 p の霧の濃さ。 玉の中ではほぼ一定で、 縁の近くで急に 0 になる。
// 大きなむら + 細かいむらのノイズを足し、 時間でゆっくり上へ流す
float FogDensity(float3 p)
{
    float r = length(p - FOG_CENTER) / FOG_RADIUS;    // 0 (真ん中) 〜 1 (縁)
    float ball = 1.0 - smoothstep(0.8, 1.0, r);       // 縁の外側 2 割で 1 → 0
    float3 flow = float3(0.0, -Time * 0.15, 0.0);
    float noise = 0.5 * ValueNoise(p * 4.0 + flow) + 0.25 * ValueNoise(p * 10.0 + flow);
    return Density * ball * saturate(0.55 + noise);
}

// 光線 ro + rd·t が玉 (球) を通る区間 [t0, t1]。 通らなければ false
bool IntersectFogBall(float3 ro, float3 rd, out float t0, out float t1)
{
    float3 oc = ro - FOG_CENTER;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - FOG_RADIUS * FOG_RADIUS;
    float h = b * b - c;
    t0 = 0.0;
    t1 = 0.0;
    if (h < 0.0) return false;
    h = sqrt(h);
    t0 = max(-b - h, 0.0);  // カメラが玉の中なら、 入口はカメラ
    t1 = -b + h;
    return t1 > t0;
}

// 点 p から太陽へ進んだとき、 霧に遮られずに届く光の割合 (2 回目のレイマーチ。 歩数は少なめ)
float SunVisibility(float3 p)
{
    float3 L = SunDirection();
    float t0, t1;
    if (!IntersectFogBall(p, L, t0, t1)) return 1.0;
    float dt = (t1 - t0) / float(SHADOW_STEPS);
    float opticalDepth = 0.0;  // 光学的な厚さ = 密度 × 長さ の合計
    [loop]
    for (int i = 0; i < SHADOW_STEPS; ++i) {
        opticalDepth += FogDensity(p + L * (t0 + (float(i) + 0.5) * dt)) * dt;
    }
    return exp(-opticalDepth);
}

// 画素の色 (表示用)
float3 ShadeFog(float2 ndc)
{
    float3 ro, rd;
    CameraRay(ndc, ro, rd);
    float3 background = Environment(ro, rd);

    float t0, t1;
    if (!IntersectFogBall(ro, rd, t0, t1)) return ToDisplay(background);

    // 一定の歩幅 dt で進む。 dt は 「玉の直径 ÷ 歩数」 なので、 歩数を増やすほど細かくなる。
    // 歩く位置は 「カメラからの距離が dt の倍数 + 半歩」 にそろえる。 歩数が少ないと、
    // カメラを中心にした玉ねぎの皮のような層 (歩幅の跡) が見える
    float dt = 2.0 * FOG_RADIUS / float(StepCount);
    float t = (floor(t0 / dt) + 0.5) * dt;     // 玉の入口の手前の、 最初の歩
    float3 scattered = float3(0.0, 0.0, 0.0);  // 霧が目の方へ散らばらせた光 (足していく)
    float transmittance = 1.0;                 // 霧に遮られずに残っている割合 (減っていく)
    [loop]
    for (int i = 0; i < MAX_STEPS + 1; ++i) {
        if (t > t1) break;  // 玉の出口を過ぎた
        float3 p = ro + rd * t;
        t += dt;
        float sigma = FogDensity(p);  // 玉の外 (入口の手前) なら 0
        if (sigma <= 0.0) continue;

        // この点を照らす光: 太陽 (影ありなら届く割合を掛ける) + 空からの弱い光
        float sun = Shadow != 0 ? SunVisibility(p) : 1.0;
        float3 light = SunColor() * sun + float3(0.16, 0.20, 0.27);

        // この 1 歩で霧に当たる割合 a = 1 − exp(−σ × dt)。 当たった光の一部が目の方へ散らばる
        float a = 1.0 - exp(-sigma * dt);
        scattered += transmittance * a * light;
        transmittance *= 1.0 - a;
        if (transmittance < 0.01) break;  // ほぼ何も通らないなら、 先を読んでも変わらない
    }
    // 霧の向こうの景色は、 遮られずに残った割合だけ見える
    return ToDisplay(scattered + transmittance * background);
}

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(ShadeFog(input.ndc), 1.0);
}
