// =============================================================================
// BakedSdf.hlsl — サンプル 05: SDF を焼いて描く (3D テクスチャを読んで球追跡)
//
// 04 では SDF の式を毎回計算した。 ここでは CPU で前もって格子に焼いた値を 3D テクスチャから読む。
// 球追跡・法線の出し方は 04 と同じで、 「Sdf(p) の中身」 が式からテクスチャに変わるだけ。
//   ・格子の点の間は、 サンプラーが線形補間 (近くの 8 点を混ぜる) で埋める
//   ・格子が粗いと、 とがった角は丸まり、 マスより細い所は消える
// 解説: samples/05_baked_sdf/README.md
// =============================================================================

#include "../../common/shaders/Fullscreen.hlsli"

// C++ の BakedSdfConstants と同じ並び
cbuffer BakedSdfConstants : register(b1)
{
    float3 BoundsMin;  // 格子が覆う箱 (ワールド)
    float  VoxelSize;  // 1 マスの大きさ
    float3 BoundsMax;
    uint   ShowGrid;   // 1 なら格子の線を表面に描く
};

Texture3D<float> SdfTexture : register(t0);  // 焼いた距離 (1 マスに float 1 つ)
SamplerState LinearClamp : register(s0);     // 線形補間・端で止める

static const int MAX_STEPS = 128;
static const float HIT_EPS = 0.0005;  // 0.5 mm

// 点 p の距離をテクスチャから読む (本体の SampleSdf と同じ)
float SampleSdf(float3 p)
{
    // 箱の中の位置を 0〜1 の uvw に直して読む。 格子の点はマスの真ん中に焼いてあるので、 テクセルの中心と合う
    float3 uvw = (p - BoundsMin) / (BoundsMax - BoundsMin);
    float d = SdfTexture.SampleLevel(LinearClamp, uvw, 0);
    // 箱の外では端の値が返るので、 箱までの距離を足す (箱から遠い点で、 表面が近く見えないように)
    float3 q = max(BoundsMin - p, p - BoundsMax);
    return d + length(max(q, 0.0));
}

// 光線と箱の交差 (スラブ法)。 箱を通る区間 [tNear, tFar] を求める (本体の IntersectBox と同じ)
bool IntersectBox(float3 ro, float3 rd, out float tNear, out float tFar)
{
    float3 s = step(0.0, rd) * 2.0 - 1.0;
    float3 invRd = 1.0 / (s * max(abs(rd), 1e-8));
    float3 t0 = (BoundsMin - ro) * invRd;
    float3 t1 = (BoundsMax - ro) * invRd;
    float3 tEnter = min(t0, t1);
    float3 tExit = max(t0, t1);
    tNear = max(max(tEnter.x, tEnter.y), tEnter.z);
    tFar = min(min(tExit.x, tExit.y), tExit.z);
    return tFar > max(tNear, 0.0);
}

// 球追跡 (04 と同じ。 本体と同じく 0.9 倍で少し控えめに進む)。 箱の中 [tStart, tEnd] だけ進む
bool SphereTrace(float3 ro, float3 rd, float tStart, float tEnd, out float tHit)
{
    float t = tStart;
    tHit = tEnd;
    [loop]
    for (int i = 0; i < MAX_STEPS; ++i) {
        float d = SampleSdf(ro + rd * t);
        if (d < HIT_EPS) {
            tHit = t;
            return true;
        }
        t += d * 0.9;
        if (t > tEnd) return false;
    }
    return false;
}

// 法線: 中心差分。 ずらす幅は 1 マス (それより細かい情報は格子に無い)
float3 SdfNormal(float3 p)
{
    float h = VoxelSize;
    float3 g = float3(SampleSdf(p + float3(h, 0.0, 0.0)) - SampleSdf(p - float3(h, 0.0, 0.0)),
                      SampleSdf(p + float3(0.0, h, 0.0)) - SampleSdf(p - float3(0.0, h, 0.0)),
                      SampleSdf(p + float3(0.0, 0.0, h)) - SampleSdf(p - float3(0.0, 0.0, h)));
    return normalize(g);
}

// 格子の線: 焼いた点を通る面 (x・y・z が一定の面) が表面と交わる所を暗くする。
// 表面とほぼ平行な面は、 面全体が線になってしまうので除く (法線のその成分が大きい軸)
float GridShade(float3 p, float3 n)
{
    float3 g = (p - BoundsMin) / VoxelSize - 0.5;  // 焼いた点の上で整数になる座標
    float3 f = abs(frac(g + 0.5) - 0.5);           // いちばん近い整数までの距離 (0〜0.5)
    float3 a = abs(n);
    f = float3(a.x > 0.8 ? 0.5 : f.x, a.y > 0.8 ? 0.5 : f.y, a.z > 0.8 ? 0.5 : f.z);
    float gridLine = 1.0 - smoothstep(0.03, 0.07, min(f.x, min(f.y, f.z)));
    return lerp(1.0, 0.35, gridLine);
}

// 形の表面の色: 拡散 + 空の光 + 鏡面 (04 と同じ簡単な光。 影は付けない)
float3 ShadeSurface(float3 n, float3 rd)
{
    float3 L = SunDirection();
    float3 albedo = float3(0.85, 0.72, 0.55);
    float sun = saturate(dot(n, L));
    float sky = 0.5 + 0.5 * n.y;
    float3 h = normalize(L - rd);
    float spec = pow(saturate(dot(n, h)), 64.0) * 0.3;
    return albedo * (SunColor() * sun + float3(0.22, 0.30, 0.42) * sky) + SunColor() * spec * sun;
}

// 画素の色 (光の強さのまま)
float3 ShadeBaked(float2 ndc)
{
    float3 ro, rd;
    CameraRay(ndc, ro, rd);
    float3 background = Environment(ro, rd);

    // 1. 光線が格子の箱を通らなければ背景
    float tNear, tFar;
    if (!IntersectBox(ro, rd, tNear, tFar)) return background;

    // 2. 箱の中だけ球追跡する (カメラが箱の中なら、 カメラから)
    float t;
    if (!SphereTrace(ro, rd, max(tNear, 0.0), tFar, t)) return background;

    // 3. 当たった点の法線で光を当てる
    float3 p = ro + rd * t;
    float3 n = SdfNormal(p);
    float3 col = ShadeSurface(n, rd);
    if (ShowGrid != 0) col *= GridShade(p, n);
    return col;
}

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(ShadeBaked(input.ndc), 1.0);
}
