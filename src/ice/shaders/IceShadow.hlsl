// =============================================================================
// IceShadow.hlsl — SDF の氷の、 色付きの影
//
// 氷の部品が自分で、 影が落ちる床の四角形を描く (エンジンの背景には手を入れない)。
//   VSMain: 頂点の番号 (0〜5) から、 影が落ちうる床の四角形 (三角形 2 枚) を作る
//   PSMain: 四角形の各画素 (床の点) から太陽へ光線を出し、 SDF の中を通った長さで太陽の光を減らして、 床を描き直す
//
// 解説: docs/08-ice-shadow.md
// =============================================================================

#include "IceSdfCommon.hlsli"

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD0;  // 床の点 (ワールド、 y = 0)
};

// 影が落ちうる床の四角形の角 (id = 0〜5 で三角形 2 枚)。
// 箱の底面の四角と、 箱の上面を太陽の向きに沿って床へ落とした四角を、 両方含む四角にする
float3 ShadowQuadCorner(int id)
{
    float3 L = SunDirection();
    float3 bmin = BoundsMin + Position;  // 箱 (ワールド)
    float3 bmax = BoundsMax + Position;
    // 高さ h の点は、 太陽と逆の向きに h / L.y × (L.x, L.z) だけずれた所に影を落とす (太陽が低いほど長い影)
    float h = max(bmax.y, 0.0);
    float2 offset = -L.xz * (h / max(L.y, 0.05));
    float2 lo = min(bmin.xz, bmin.xz + offset);
    float2 hi = max(bmax.xz, bmax.xz + offset);
    //  頂点   0   1   2   3   4   5
    //  u      0   1   1   0   1   0     (三角形 0-1-2 と 3-4-5)
    //  v      0   0   1   0   1   1
    float u = (id == 1 || id == 2 || id == 4) ? 1.0 : 0.0;
    float v = (id == 2 || id == 4 || id == 5) ? 1.0 : 0.0;
    float2 xz = lerp(lo, hi, float2(u, v));
    return float3(xz.x, 0.0, xz.y);
}

VSOutput VSMain(uint id : SV_VertexID)
{
    VSOutput output;
    output.worldPos = ShadowQuadCorner(int(id));
    output.position = mul(float4(output.worldPos, 1.0), ViewProj);
    return output;
}

// 床の点 worldP から太陽へ向かう光が、 氷を通り抜けて残る割合 (RGB)。 氷が無ければ 1
float3 IceShadowTransmittance(float3 worldP)
{
    float3 ro = worldP - Position;  // 物体の空間
    float3 rd = SunDirection();
    float tNear, tFar;
    if (!IntersectBox(ro, rd, tNear, tFar)) return float3(1.0, 1.0, 1.0);

    // 太陽の向きに SDF を進み、 中 (φ < 0) を通った長さと、 気泡の濃さの合計を数える。
    // 外では φ ずつ、 中では −φ ずつ進むと表面を越えない。 表面の近くで止まらないよう、 格子の半分を下限にする
    float t = max(tNear, 0.0);
    float insideLength = 0.0;
    float bubbleDepth = 0.0;  // 光学的な厚さ: 密度 × 長さの合計
    [loop]
    for (int i = 0; i < 64; ++i) {
        if (t > tFar) break;
        float3 q = ro + rd * t;
        float d = SampleSdf(q);
        float s = max(abs(d), VoxelSize * 0.5);
        if (d < 0.0) {
            insideLength += s;
            bubbleDepth += BubbleDensityAt(q, -d) * s;
        }
        t += s;
    }
    if (insideLength <= 0.0) return float3(1.0, 1.0, 1.0);

    // 吸収 (Beer–Lambert) と、 気泡に遮られた分。 0.9 は入るときと出るときの表面の反射の分をまとめた近似
    return exp(-Absorption * insideLength - bubbleDepth) * 0.9;
}

// 床の点の色 (表示用)。 影が無い所は false (描かない = 背景のまま)
bool ShadeShadowedFloor(float3 worldPos, out float3 color)
{
    color = float3(0.0, 0.0, 0.0);
    if (CameraPos.y <= 0.0) return false;  // 床より下から見ているときは、 床は見えない

    float3 T = IceShadowTransmittance(worldPos);
    if (min(T.x, min(T.y, T.z)) >= 0.999) return false;

    // 背景 (Environment) と同じ式で床を描き直す。 違うのは太陽の光に T を掛けることだけ (空からの光はそのまま)
    float3 toPoint = worldPos - CameraPos;
    float dist = length(toPoint);
    color = ToDisplay(ApplyFog(ShadeFloorLit(worldPos, T), toPoint / dist, dist));
    return true;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float3 color;
    if (!ShadeShadowedFloor(input.worldPos, color)) discard;
    return float4(color, 1.0);
}
