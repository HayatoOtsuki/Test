// =============================================================================
// SdfRaymarch.hlsl — サンプル 04: SDF × レイマーチング (球追跡で形を描く)
//
// 形の式は 02 と同じ SdfShapes.hlsli を使う。 03 のレイマーチングの歩幅を 「その場の SDF の値」 にすると、
// 表面を越えずに、 何も無い所は大股で、 表面の近くは小股で進める (球追跡, sphere tracing)。
//   ・当たった点で SDF の傾きから法線を出し、 光を当てる
//   ・影も、 当たった点 (や床) から太陽へもう一度球追跡して調べる
//   ・表示を切り替えると、 1 画素に何歩かかったかを色で見られる
// 解説: samples/04_sdf_raymarch/README.md
// =============================================================================

#include "../../common/shaders/Fullscreen.hlsli"
#include "../../02_sdf/shaders/SdfShapes.hlsli"

// C++ の SdfRaymarchConstants と同じ並び
cbuffer SdfRaymarchConstants : register(b1)
{
    uint  Shape;      // 形の番号 (SdfShapes.hlsli の SceneSdf)
    uint  ViewMode;   // 0 = 光を当てる、 1 = 歩数を色で、 2 = 法線を色で
    uint  ShadowMode; // 0 = 影なし、 1 = くっきりした影、 2 = やわらかい影
    float StepScale;  // 歩幅の倍率 k。 1 歩 = SDF の値 × k
};

static const int MAX_STEPS = 128;     // 1 本の光線で進む歩数の上限
static const float HIT_EPS = 0.001;   // 表面までこれより近ければ 「当たった」 (1 mm)
static const float FAR_DIST = 20.0;   // 床にも形にも当たらないとき、 ここであきらめる
static const int SHADOW_STEPS = 64;
static const float SOFTNESS = 8.0;    // やわらかい影の境目の広さ。 大きいほどくっきり

float Sdf(float3 p)
{
    return SceneSdf(p, int(Shape));
}

// 球追跡: ro から rd へ、 tMax まで進んで形の表面を探す。 当たれば true と、 その t。
// steps には、 かかった歩数を入れる (表示用)
bool SphereTrace(float3 ro, float3 rd, float tMax, out float t, out int steps)
{
    t = 0.0;
    steps = 0;
    [loop]
    for (int i = 0; i < MAX_STEPS; ++i) {
        steps = i + 1;
        float d = Sdf(ro + rd * t);
        // 表面まで 1 mm 未満なら当たり。 k > 1 で表面を越えて中に入った (d < 0) ときも、 ここで止まる
        if (d < HIT_EPS) return true;
        // 距離 d の球の中には表面が無いので、 d だけ進んでも表面を越えない
        t += d * StepScale;
        if (t > tMax) break;
    }
    return false;
}

// 法線: SDF の傾き (勾配)。 各軸で少し前と少し後ろの差を取る (中心差分)
float3 SdfNormal(float3 p)
{
    const float h = 0.0005;
    float3 g = float3(Sdf(p + float3(h, 0.0, 0.0)) - Sdf(p - float3(h, 0.0, 0.0)),
                      Sdf(p + float3(0.0, h, 0.0)) - Sdf(p - float3(0.0, h, 0.0)),
                      Sdf(p + float3(0.0, 0.0, h)) - Sdf(p - float3(0.0, 0.0, h)));
    return normalize(g);
}

// 点 p から太陽へ球追跡して、 光が届く割合 (0〜1)。
// やわらかい影: 当たらなかった光線でも、 途中で形のすぐ近く (d が小さい) を通ったなら、 その分だけ暗くする
float SunVisibility(float3 p)
{
    if (ShadowMode == 0) return 1.0;
    float3 L = SunDirection();
    float visibility = 1.0;
    float t = 0.01;  // 出発点の表面に、 自分で当たらないよう少し離れてから
    [loop]
    for (int i = 0; i < SHADOW_STEPS; ++i) {
        float d = Sdf(p + L * t);
        if (d < HIT_EPS) return 0.0;  // 形に遮られた
        // d / t: 光線から形までの隙間を、 ここまでの距離で割った値 (= 隙間が見える角度の目安)
        if (ShadowMode == 2) visibility = min(visibility, SOFTNESS * d / t);
        t += d;
        if (t > 3.0) break;  // 形の大きさより十分遠くまで来た
    }
    return saturate(visibility);
}

// 歩数を色にする (0 = 青 → 緑 → 黄 → 1 = 赤)
float3 HeatColor(float x)
{
    const float3 blue = float3(0.10, 0.20, 0.80);
    const float3 green = float3(0.10, 0.75, 0.30);
    const float3 yellow = float3(1.00, 0.85, 0.10);
    const float3 red = float3(0.90, 0.10, 0.10);
    x = saturate(x) * 3.0;
    if (x < 1.0) return lerp(blue, green, x);
    if (x < 2.0) return lerp(green, yellow, x - 1.0);
    return lerp(yellow, red, x - 2.0);
}

// 形の表面の色: 拡散 (Lambert) + 空からの光 + 鏡面 (Blinn-Phong)
float3 ShadeSurface(float3 p, float3 n, float3 rd)
{
    float3 L = SunDirection();
    float3 albedo = float3(0.70, 0.80, 0.92);
    float sun = SunVisibility(p + n * 0.002) * saturate(dot(n, L));
    float sky = 0.5 + 0.5 * n.y;  // 上を向くほど空の光を受ける
    float3 h = normalize(L - rd);
    float spec = pow(saturate(dot(n, h)), 64.0) * 0.3;
    return albedo * (SunColor() * sun + float3(0.22, 0.30, 0.42) * sky) + SunColor() * spec * sun;
}

// 画素の色 (光の強さのまま)
float3 ShadeSdfScene(float2 ndc)
{
    float3 ro, rd;
    CameraRay(ndc, ro, rd);

    // 床 (y = 0) に当たる距離。 形は床より上にあるので、 球追跡はそこまででよい
    bool floorHit = rd.y < 0.0;
    float tFloor = floorHit ? -ro.y / rd.y : FAR_DIST;

    float t;
    int steps;
    bool hit = SphereTrace(ro, rd, min(tFloor, FAR_DIST), t, steps);

    if (ViewMode == 1) return FromDisplay(HeatColor(float(steps) / 64.0));  // 64 歩で赤 (デバッグの色は戻して出す)
    if (hit) {
        float3 p = ro + rd * t;
        float3 n = SdfNormal(p);
        if (ViewMode == 2) return FromDisplay(n * 0.5 + 0.5);
        return ShadeSurface(p, n, rd);
    }
    if (ViewMode == 2) return float3(0.0, 0.0, 0.0);
    if (!floorHit) return Sky(rd);

    // 床: 太陽への球追跡で、 形の影を落とす
    float3 pf = ro + rd * tFloor;
    float sunVisibility = SunVisibility(pf);
    float3 floorColor = ShadeFloorLit(pf, float3(sunVisibility, sunVisibility, sunVisibility));
    return ApplyFog(floorColor, rd, tFloor);
}

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(ShadeSdfScene(input.ndc), 1.0);
}
