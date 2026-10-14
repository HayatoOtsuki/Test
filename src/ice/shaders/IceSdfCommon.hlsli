// =============================================================================
// IceSdfCommon.hlsli — SDF の氷のシェーダー (IceSdf.hlsl) と影のシェーダー (IceShadow.hlsl) が共有する物
//
//   ・SDF の 3D テクスチャとサンプラー
//   ・SDF を読む・法線・光線と箱の交差・球追跡
//   ・気泡の密度
// =============================================================================

#ifndef ICE_SDF_COMMON_HLSLI
#define ICE_SDF_COMMON_HLSLI

#include "IceCommon.hlsli"

// SDF の 3D テクスチャ (generateSDF で作る)。 1 つの値 = その点から氷の表面までの符号付きの距離 (メートル)
Texture3D<float> SdfTexture : register(t0);
// 線形補間・端で止める (ルートシグネチャの静的サンプラー)
SamplerState LinearClamp : register(s0);

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

#endif
