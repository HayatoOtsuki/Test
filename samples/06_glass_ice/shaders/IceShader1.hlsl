// =============================================================================
// IceShader1.hlsl — 氷 段階 1: 形を探す (球追跡)
//
// 作る順に 1 段ずつ足していくシェーダーの、 途中の段。 完成形は IceShader13.hlsl (参考文献と全部の解説もそこ)。
// 前の段から足した行には 「★ 段階 N」 と書いてある。 前の段のファイルと見比べると、 足した所が分かる。
//
// 【段階】
//    0. 箱を描く
//    1. 形を探す (球追跡)  ← 今回
//    2. 法線と光 (不透明)
//    3. 透明にする (映り込みと透過)
//    4. 屈折 (歪み)
//    5. 全反射と後ろの面
//    6. 吸収と溶けかけの形
//    7. 白い芯 (内部散乱の簡単版)
//    8. 気泡
//    9. 亀裂
//   10. 体積の散乱 (層 × ノイズ)
//   11. 散乱の影 (太陽への光)
//   12. 前方散乱
//   13. 周りの景色 (画面のコピー、 完成)
//
// 【前の段から引き継ぐ物】
//   箱を描く頂点シェーダー (全部の段階で同じ)。
//
// 【今回足す物】
//   ・sdRoundBox / sdf: 氷の形 (角の丸い立方体) の距離の式
//   ・rayMarch: 球追跡で、 外から形の表面を探す
//   ・ピクセルシェーダー: 光線を作り、 表面に当たったら白く塗る。 当たらない画素は discard。 当たった点の深度 (SV_Depth) を出す
//
// 【仕組み】
//   光線: 最初に 1 回だけ、 カメラを部品の空間へ直す (逆行列 invWorld)。 向きは、 カメラから箱の裏面の点 v.LPos へ。
//   球追跡: SDF の値 φ は 「いちばん近い表面までの距離」。 点 p を中心に半径 φ の球の中には表面が無いので、 φ だけ進んでも表面を越えない。
//     何も無い所は大股、 表面の近くは小股で進み、 0.2 mm より近づいたら当たり。 箱の裏面まで進んだら、 その画素に形は無い。
//   深度: 箱の面の深度のままだと、 重なった部品の前後が箱の大きさで決まってしまう。 当たった点を画面へ変換して、 その z を出す。
//
// 【確かめること】
//   ・箱の代わりに、 角の丸い白い立方体が 3 つ見える (陰は無いので、 輪郭だけが分かる)
//   ・上の氷が下の氷の手前・奥に、 正しく重なる (深度)
//
// 【参考】
//   Inigo Quilez "distance functions" (Round Box)  https://iquilezles.org/articles/distfunctions
//   Inigo Quilez "raymarching SDFs" (球追跡)       https://iquilezles.org/articles/raymarchingdf
// =============================================================================

#include "../../../src/engine/shaders/SceneConstants.hlsli"  // シーンの定数 (ViewProj・LightDirection・LightColor)

// 部品 1 つ分の定数。 C++ の SdfObject の ObjectConstants と同じ並び。 全部の段階で同じ (C++ は段階で変わらない)
cbuffer ObjectConstants : register(b1)
{
    float4x4 world;        // 部品の空間 → ワールドの行列 (転置済み。 mul(float4(p, 1), world) と書く)
    float4x4 invWorld;     // ワールド → 部品の空間の行列 (world の逆行列。 ピクセルシェーダーの最初に 1 回だけ使う)
    float3   cameraPos;    // カメラの位置 (ワールド)
    float    ior;          // 屈折率 n (氷 1.31)。 段階 3 から使う
    float3   absorption;   // 1 m あたりの吸収の強さ σa (赤・緑・青)。 段階 6 から使う
    float    coreDensity;  // 白い芯の気泡の濃さ σs (1/m)。 0 なら芯なし。 段階 7 から使う
};

static const float ICE_HALF = 0.13;           // 氷の半分の大きさ (m)。 1 辺 26 cm (場面は本物の約 8 倍)
static const float ICE_ROUND = 0.035;         // 氷の角の丸みの半径 (m)
static const float HIT_EPS = 0.0002;          // 表面まで、 これより近ければ 「当たった」 (m)。 0.2 mm

// ---- SDF (形): 点 p から形の表面までの符号付きの距離。 外が正、 表面が 0、 中が負 ----

// 角の丸い箱 (半分の大きさ b、 丸み r): 箱を r だけ小さくし、 表面を r だけふくらませる
float sdRoundBox(float3 p, float3 b, float r)
{
    float3 q = abs(p) - (b - r);                                          // 小さくした箱の各面から、 どれだけ外へ出ているか (負なら中)
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;  // 外: 出ている成分の長さ、 中: いちばん近い面まで。 − 丸み
}

// 氷の形 (部品の空間。 形の真ん中が原点)
float sdf(float3 p)
{
    float d = sdRoundBox(p, float3(ICE_HALF, ICE_HALF, ICE_HALF), ICE_ROUND);  // 角の丸い立方体
    return d;
}

// ---- レイマーチング ----

// 外から表面を探す (球追跡): SDF の値 φ は 「いちばん近い表面までの距離」 なので、 φ だけ進めば表面を越えない。
// tMax まで進んで当たらなければ false。 t: 表面までの距離
bool rayMarch(float3 ro, float3 rd, float tMax, out float t)
{
    t = 0.0;
    [loop]
    for (int i = 0; i < 128; ++i) {          // 歩数の上限
        float d = sdf(ro + rd * t);          // 今いる点から、 いちばん近い表面までの距離
        if (d < HIT_EPS) return true;        // 表面まで 0.2 mm 未満なら当たり
        t += d * 0.9;                        // 0.9 倍で控えめに進む (でこぼこを足した SDF は、 本当の距離より少し大きいことがある)
        if (t > tMax) break;                 // 箱の裏面まで来た = 当たらない
    }
    return false;
}

// ---- 頂点シェーダー: 形を囲む箱 (全部の段階で同じ) ----

struct VSInput
{
    float3 pos : POSITION;   // 箱の頂点 (部品の空間)
    float2 uv  : TEXCOORD0;  // 箱の面の UV
};

struct PSInput
{
    float4 Pos  : SV_Position;  // 画面の位置
    float3 LPos : TEXCOORD0;    // 部品の空間の位置 (形の真ん中が原点)
    float3 WPos : TEXCOORD1;    // ワールドの位置
    float2 UV   : TEXCOORD2;    // 箱の面の UV (このシェーダーでは使わない)
};

PSInput VSMain(VSInput input)
{
    PSInput v;
    v.LPos = input.pos;                                    // 部品の空間 (変換する前)
    v.WPos = mul(float4(input.pos, 1.0), world).xyz;       // 部品の空間 → ワールド (最初に 1 回だけ変換する)
    v.Pos = mul(float4(v.WPos, 1.0), ViewProj);            // ワールド → 画面
    v.UV = input.uv;
    return v;
}

// ---- ピクセルシェーダー ----

float4 PSMain(PSInput v, out float depth : SV_Depth) : SV_Target
{
    // 最初に 1 回だけ、 カメラを部品の空間へ (逆行列 invWorld)。 ここから先は全部、 部品の空間で計算する
    float3 ro = mul(float4(cameraPos, 1.0), invWorld).xyz;   // 光線の出発点 (部品の空間のカメラ。 点なので w = 1)
    float3 rd = normalize(v.LPos - ro);                      // 光線の向き: カメラ → この画素 (箱の裏面の点)

    // 1. 外から表面を探す。 箱の裏面まで進んでも当たらなければ、 この画素は描かない (後ろの物が残る)
    float t;                                                 // カメラから表面までの距離
    if (!rayMarch(ro, rd, length(v.LPos - ro), t)) discard;
    float3 p = ro + rd * t;                                  // 当たった点 (部品の空間)
    float4 clip = mul(mul(float4(p, 1.0), world), ViewProj); // 当たった点: 部品の空間 → ワールド → 画面
    depth = clip.z / clip.w;                                 // 深度 = 透視の割り算の後の z (0〜1)。 箱の面ではなく当たった点の深度

    return float4(1.0, 1.0, 1.0, 1.0);  // 段階 1: 当たった所を白く塗るだけ (形の輪郭と深度を確かめる)
}
