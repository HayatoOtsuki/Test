// =============================================================================
// SdfSlice.hlsl — サンプル 02: SDF だけ (距離場の断面を色で見る)
//
// 空間に 1 枚の 「断面の板」 を置き、 板の上の各点で SDF (表面までの符号付きの距離) を計算して色を塗る。
//   外 (φ > 0) は暖色、 中 (φ < 0) は青、 表面 (φ = 0) は濃い線、 0.05 ごとに等高線
// 光線は板に当てるだけで、 形の表面は探さない (レイマーチングは使わない)。
// 板は ↑ ↓ で動き、 P で縦 (z が一定の面) と横 (y が一定の面) を切り替える。
// =============================================================================

#include "../../common/shaders/Fullscreen.hlsli"
#include "SdfShapes.hlsli"

// C++ の SdfSliceConstants と同じ並び
cbuffer SdfSliceConstants : register(b1)
{
    uint  Shape;        // 形の番号 (SdfShapes.hlsli の SceneSdf)
    float Slice;        // 板の位置 (縦なら z、 横なら y)
    uint  Horizontal;   // 0 = 縦の板 (z = Slice)、 1 = 横の板 (y = Slice)
    float SlicePad;
};

static const float PLANE_HALF = 1.0;  // 板の大きさ (半分、 メートル)

// 距離 d を色にする (光の強さではなく、 そのまま画面の色)
float3 DistanceColor(float d)
{
    // 等高線: 0.05 ごとに少し暗い線。 frac(d / 間隔) が 0 に近い所
    float bands = abs(frac(d / 0.05 + 0.5) - 0.5) * 2.0;           // 線の真ん中で 0、 線と線の真ん中で 1
    float contour = smoothstep(0.0, 0.12, bands);
    float3 outside = lerp(float3(0.98, 0.86, 0.70), float3(0.80, 0.42, 0.20), saturate(d / 0.6));   // 遠いほど濃い暖色
    float3 inside = lerp(float3(0.62, 0.80, 0.96), float3(0.05, 0.25, 0.60), saturate(-d / 0.4)); // 深いほど濃い青
    float3 col = (d > 0.0 ? outside : inside) * lerp(0.82, 1.0, contour);
    // 表面 (d = 0): 太い濃い線。 幅は画素の大きさに合わせず、 0.006 m で決め打ち
    float surface = 1.0 - smoothstep(0.004, 0.008, abs(d));
    return lerp(col, float3(0.05, 0.05, 0.08), surface);
}

// 画素の色 (表示用)。 板に当たらなければ背景 (空と床)
float3 ShadeSlice(float2 ndc)
{
    float3 ro, rd;
    CameraRay(ndc, ro, rd);

    // 光線と板の交差: 板は 「ある軸の値が Slice の平面」。 その軸の成分で割れば、 当たる t が出る
    float o = Horizontal != 0 ? ro.y : ro.z;
    float v = Horizontal != 0 ? rd.y : rd.z;
    float t = (Slice - o) / (abs(v) > 1e-5 ? v : 1e-5);
    float3 p = ro + rd * t;

    // 板の範囲: 縦なら x と y、 横なら x と z
    float2 onPlane = Horizontal != 0 ? float2(p.x, p.z) : float2(p.x, p.y - SHAPE_CENTER.y);
    bool inside = t > 0.0 && abs(onPlane.x) < PLANE_HALF && abs(onPlane.y) < PLANE_HALF;
    if (!inside) return ToDisplay(Environment(ro, rd));

    float3 col = DistanceColor(SceneSdf(p, int(Shape)));
    // 板の縁を細い線で
    float edge = max(abs(onPlane.x), abs(onPlane.y));
    col = lerp(col, float3(0.2, 0.2, 0.25), smoothstep(PLANE_HALF - 0.01, PLANE_HALF, edge));
    return col;
}

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(ShadeSlice(input.ndc), 1.0);
}
