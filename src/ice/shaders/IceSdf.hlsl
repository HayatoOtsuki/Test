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

#include "IceCommon.hlsli"

// SDF の 3D テクスチャ (generateSDF で作る)。 1 つの値 = その点から氷の表面までの符号付きの距離 (メートル)
Texture3D<float> SdfTexture : register(t0);
// 線形補間・端で止める (ルートシグネチャの静的サンプラー)
SamplerState LinearClamp : register(s0);

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
// SDF を読む
// -----------------------------------------------------------------------------

// 物体の空間の点 p での SDF の値 (表面までの符号付きの距離。 外が正、 中が負)
float SampleSdf(float3 p)
{
    // 箱の中の位置を 0〜1 の uvw に直して、 3D テクスチャを線形補間で読む。
    // 格子の点は、 箱を等分したマス目の真ん中に焼いてあるので、 テクスチャの読み方 (テクセルの中心) と合う
    float3 uvw = (p - BoundsMin) / (BoundsMax - BoundsMin);
    float d = SdfTexture.SampleLevel(LinearClamp, uvw, 0);

    // 箱の外では、 サンプラーが端の値を返す (端で止める)。 そのままだと、 箱から遠い点でも表面が近く見えてしまうので、
    // 箱までの距離を足す。 q は 「箱からどれだけ出ているか」 を成分ごとに見たもの
    float3 q = max(BoundsMin - p, p - BoundsMax);
    return d + length(max(q, 0.0));
}

// 法線 = SDF の勾配 (距離がいちばん速く増える向き)。
// 各軸の向きに格子 1 つ分だけ前後にずらして読み、 差を取る (中心差分)。 長さ 1 に直す
float3 SdfNormal(float3 p)
{
    float h = VoxelSize;
    float3 g = float3(SampleSdf(p + float3(h, 0.0, 0.0)) - SampleSdf(p - float3(h, 0.0, 0.0)),
                      SampleSdf(p + float3(0.0, h, 0.0)) - SampleSdf(p - float3(0.0, h, 0.0)),
                      SampleSdf(p + float3(0.0, 0.0, h)) - SampleSdf(p - float3(0.0, 0.0, h)));
    return normalize(g);
}

// -----------------------------------------------------------------------------
// 光線と箱
// -----------------------------------------------------------------------------

// 光線 ro + rd·t が箱 [BoundsMin, BoundsMax] を通る区間 [tNear, tFar] を求める (スラブ法)。
// 箱を 「x の板・y の板・z の板の重なり」 と考え、 それぞれの板に入る t と出る t を求める。
// 全部の板に入った後 (入る t の最大) から、 どれかの板を出るまで (出る t の最小) が箱の中
bool IntersectBox(float3 ro, float3 rd, out float tNear, out float tFar)
{
    // 向きの成分が 0 だと 0 で割ってしまうので、 符号を保ったまま、 ごく小さい値で止める
    float3 s = step(0.0, rd) * 2.0 - 1.0;  // 0 以上なら +1、 負なら -1
    float3 invRd = 1.0 / (s * max(abs(rd), 1e-8));

    float3 t0 = (BoundsMin - ro) * invRd;  // 各軸で、 小さい側の板の面に届く t
    float3 t1 = (BoundsMax - ro) * invRd;  // 大きい側の板の面に届く t
    float3 tEnter = min(t0, t1);           // 光線の向きによって、 どちらが入口かが変わる
    float3 tExit = max(t0, t1);
    tNear = max(max(tEnter.x, tEnter.y), tEnter.z);
    tFar = min(min(tExit.x, tExit.y), tExit.z);
    return tFar > max(tNear, 0.0);  // 区間があり、 カメラより前にある
}

// -----------------------------------------------------------------------------
// 球追跡 (sphere tracing)
// -----------------------------------------------------------------------------

// 光線 ro + rd·t の区間 [tStart, tEnd] で、 SDF の表面 (値が 0) に当たる t を探す。
// SDF の値 d は 「いちばん近い表面までの距離」 なので、 今の点から半径 d の球の中には表面が無い。
// だから d だけ進んでも表面を通り越さない。 これを、 d が十分小さくなる (当たった) か、 区間を出る (外れた) まで繰り返す
bool SphereTrace(float3 ro, float3 rd, float tStart, float tEnd, out float tHit)
{
    float t = tStart;
    tHit = tEnd;
    [loop]
    for (int i = 0; i < 128; ++i) {
        float d = SampleSdf(ro + rd * t);
        if (d < 0.0005) {  // 0.5 mm より近ければ当たったことにする
            tHit = t;
            return true;
        }
        // 0.9 を掛けて少し控えめに進む。 表面のゆらぎ (ノイズ) で距離が厳密でない分の安全のため
        t += d * 0.9;
        if (t > tEnd) return false;
    }
    return false;
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

// ---- 気泡 (芯の白い濁り) ----

// 点 p (物体の空間)、 表面からの深さ depth (メートル) での気泡の密度 (1/m)。
// 冷凍庫の氷は外側から凍り、 溶けていた空気が最後に凍る芯に集まる。 だから 「表面から 10 cm より深い所」 だけに気泡を置く。
// 深さ 10〜22 cm でなめらかに増やし、 正弦波を掛けた模様で、 均一でない (筋やむらのある) 濁りにする
float BubbleDensityAt(float3 p, float depth)
{
    float core = smoothstep(0.10, 0.22, depth);
    float pattern = 0.55 + 0.45 * sin(p.x * 37.0) * sin(p.y * 41.0 + 1.7) * sin(p.z * 39.0 + 0.6);
    return BubbleDensity * core * pattern;
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

// この画素の色 (表示用)。 氷に当たらなければ false を返す
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

    color = ToDisplay(col);
    return true;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float3 color;
    if (!ShadeIceSdf(input.worldPos, color)) discard;  // 氷に当たらない画素は描かない (背景が見える)
    return float4(color, 1.0);
}
