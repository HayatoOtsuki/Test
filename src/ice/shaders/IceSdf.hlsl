// =============================================================================
// IceSdf.hlsl — SDF (符号付き距離場) の氷
//
//   VSMain: 格子が覆う箱のメッシュを、 プリミティブと同じようにクリップ空間へ移す
//   PSMain: 箱が映る画素ごとに、 カメラからの光線で箱の中の SDF を球追跡し、 氷の表面を探す。
//           当たらなければ捨てる (discard) ので、 背景がそのまま見える
//
// この段 (generateSDF) では形を確かめるため、 表面をプリミティブと同じ PBR で不透明に塗る。
// 反射・屈折・吸収は feature/ice-optics で足す。
// 解説: docs/05-generate-sdf.md
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

// この画素の色 (表示用)。 氷に当たらなければ false を返す
bool ShadeIceSdf(float3 worldPos, out float3 color)
{
    color = float3(0.0, 0.0, 0.0);

    // 光線は物体の空間で追う。 氷は回さないので、 ワールドから位置 Position を引くだけで物体の空間になる
    float3 ro = CameraPos - Position;
    float3 rd = normalize(worldPos - CameraPos);

    // 1. 箱に入る所から出る所までだけを調べる。 カメラが箱の中なら、 入口はカメラ (t = 0)
    float tNear, tFar;
    if (!IntersectBox(ro, rd, tNear, tFar)) return false;

    // 2. 箱の中を球追跡して、 表面を探す
    float tHit;
    if (!SphereTrace(ro, rd, max(tNear, 0.0), tFar, tHit)) return false;

    // 3. 当たった点の法線で塗る。 この段はプリミティブと同じ PBR (不透明)
    float3 p = ro + rd * tHit;
    color = ShadeStandard(p + Position, SdfNormal(p));
    return true;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float3 color;
    if (!ShadeIceSdf(input.worldPos, color)) discard;  // 氷に当たらない画素は描かない (背景が見える)
    return float4(color, 1.0);
}
