// =============================================================================
// SdfShapes.hlsli — SDF の式 (形の距離) と、 形の組み合わせ (CSG)
//
// どの関数も 「点 p から形の表面までの符号付きの距離」 を返す。 外が正、 表面が 0、 中が負。
// 形はどれも、 床 (y = 0) の上の真ん中あたりに置いてある (中心 (0, 0.45, 0))。
// 解説: samples/02_sdf/README.md
// =============================================================================

#ifndef SDF_SHAPES_HLSLI
#define SDF_SHAPES_HLSLI

static const float3 SHAPE_CENTER = float3(0.0, 0.45, 0.0);

// ---- 基本の形 ----

// 球: 中心までの距離から半径を引く
float sdSphere(float3 p, float r)
{
    return length(p) - r;
}

// 箱 (半分の大きさ b)。 q = 各軸で面からどれだけ外へ出ているか。
// 外: 出ている成分だけの長さ。 中: いちばん近い面までの距離 (負)
float sdBox(float3 p, float3 b)
{
    float3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// 角を丸めた箱: 箱を r だけ小さくしてから、 表面を r だけ外へふくらませる
float sdRoundBox(float3 p, float3 b, float r)
{
    return sdBox(p, b - r) - r;
}

// トーラス (ドーナツ。 y 軸の周りの輪)。 R: 輪の半径、 r: 管の太さ。
// 点を 「輪の上のいちばん近い点」 からの 2 次元の距離に直すと、 ただの円 (半径 r) になる
float sdTorus(float3 p, float R, float r)
{
    float2 q = float2(length(p.xz) - R, p.y);
    return length(q) - r;
}

// ---- 組み合わせ (CSG) ----

// 足す (A または B): 近い方の表面
float opUnion(float a, float b)
{
    return min(a, b);
}

// 削る (A から B を除く): 「A の中」 かつ 「B の外」。 B の符号を反転すると、 B の外が中になる
float opSubtract(float a, float b)
{
    return max(a, -b);
}

// なめらかに足す: 2 つが k より近い所だけ、 少し小さくして境目を丸める (2 次式のなめらかな最小)
float opSmoothUnion(float a, float b, float k)
{
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k * 0.25;
}

// なめらかに削る: なめらかな最大で、 削った縁を丸める
float opSmoothSubtract(float a, float b, float k)
{
    float h = max(k - abs(a + b), 0.0) / k;
    return max(a, -b) + h * h * k * 0.25;
}

// ---- 本体の氷と同じ形 (角の丸い箱 + 角の欠け + 表面のゆらぎ) ----

// 整数の格子点の決まった乱数 (0〜1)
float Hash3(float3 i)
{
    return frac(sin(dot(i, float3(127.1, 311.7, 74.7))) * 43758.5453);
}

// 値ノイズ: 格子点の乱数の間を、 なめらかな重み t²(3 − 2t) で 3 方向に補間した −1〜1 の値
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

// 氷の塊: 本体の iceShapeSdf と同じ作り (乱数の作り方だけ違う)
float sdIce(float3 p)
{
    float3 q = p - float3(0.0, 0.38, 0.0);
    float d = sdRoundBox(q, float3(0.38, 0.38, 0.38), 0.08);
    d = opSmoothSubtract(d, sdSphere(p - float3(0.38, 0.76, 0.38), 0.20), 0.05);  // 上の角を 1 つ欠く
    float n = ValueNoise(p * 9.0) * 0.7 + ValueNoise(p * 23.0) * 0.3;
    return d + n * 0.006;  // 6 mm のゆらぎ
}

// ---- 形の番号 → SDF ----

static const int SHAPE_COUNT = 8;

float SceneSdf(float3 p, int shape)
{
    float3 c = p - SHAPE_CENTER;
    if (shape == 0) return sdSphere(c, 0.4);
    if (shape == 1) return sdBox(c, float3(0.35, 0.35, 0.35));
    if (shape == 2) return sdRoundBox(c, float3(0.35, 0.35, 0.35), 0.1);
    if (shape == 3) return sdTorus(c, 0.32, 0.12);
    if (shape == 4) return opUnion(sdBox(c - float3(-0.15, 0.0, 0.0), float3(0.25, 0.25, 0.25)), sdSphere(c - float3(0.25, 0.0, 0.0), 0.3));
    if (shape == 5) return opSubtract(sdBox(c, float3(0.35, 0.35, 0.35)), sdSphere(c, 0.45));
    if (shape == 6) return opSmoothUnion(sdSphere(c - float3(-0.2, 0.0, 0.0), 0.28), sdSphere(c - float3(0.22, 0.05, 0.0), 0.22), 0.25);
    return sdIce(p);
}

#endif
