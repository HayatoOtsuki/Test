// =============================================================================
// IceShader2.hlsl — 氷 段階 2: 法線と光 (不透明)
//
// 作る順に 1 段ずつ足していくシェーダーの、 途中の段。 完成形は IceShader13.hlsl (参考文献と全部の解説もそこ)。
// 前の段から足した行には 「★ 段階 N」 と書いてある。 前の段のファイルと見比べると、 足した所が分かる。
//
// 【段階】
//    0. 箱を描く
//    1. 形を探す (球追跡)
//    2. 法線と光 (不透明)  ← 今回
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
//   形 (sdf)、 球追跡 (rayMarch)、 光線の作り方、 discard と深度。
//
// 【今回足す物】
//   ・calcNormal: 法線 = SDF の傾き (中心差分)
//   ・ピクセルシェーダー: 法線で太陽の光を当てる (ランバート)。 不透明な形として見る
//
// 【仕組み】
//   法線: SDF は表面から離れる向きにいちばん速く増える。 その向き (勾配) が外向きの法線。
//     各軸で少し前と少し後ろの SDF の差を取る (中心差分)。 長さは normalize でそろえる。
//   ランバート: 面の明るさ = cosθ = n·L (n: 法線、 L: 太陽への向き)。 太陽はワールドの向きなので、 法線をワールドへ直してから比べる。
//     太陽の向きと色は、 シーンの定数バッファの LightDirection / LightColor を直接読む (自分のエンジンのライトの定数に当たる)。
//     向きの変換は w = 0 (移動の部分を効かせない)。
//   透明にする前に、 法線が正しいかを不透明で確かめておく段。 後の段は全部この法線を使う。
//
// 【確かめること】
//   ・太陽の側が明るく、 反対側が暗い。 角の丸みがなめらかに明るさを変える
//   ・J / L で太陽を回すと、 明るい面が回る
//
// 【参考】
//   Inigo Quilez "normals for an SDF"  https://iquilezles.org/articles/normalsSDF
//
// 【コードの引用元】
//   calcNormal: iq "normals for an SDF" の中心差分と同じ形 (e.xyy のような書き方も記事の流儀)。 ずらす幅 0.5 mm は、 このサンプルで決めた
//   ランバート (n·L) と空の光: 自分で書いた。 色の値は確かめる用に決めた
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

// 法線 = SDF の傾き (勾配)。 各軸で少し前と少し後ろの差を取る (中心差分)。 2h で割るのは normalize があるので省く
float3 calcNormal(float3 p)
{
    const float2 e = float2(0.0005, 0.0);  // ずらす幅 h = 0.5 mm (e.xyy = (h, 0, 0)、 e.yxy = (0, h, 0)、 e.yyx = (0, 0, h))
    return normalize(float3(sdf(p + e.xyy) - sdf(p - e.xyy),    // ∂φ/∂x
                            sdf(p + e.yxy) - sdf(p - e.yxy),    // ∂φ/∂y
                            sdf(p + e.yyx) - sdf(p - e.yyx)));  // ∂φ/∂z
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

    // ★ 段階 2: 法線で光を当てる (ランバートの余弦則)。 不透明な形として見る
    float3 n = calcNormal(p);                                                 // 法線 (部品の空間)
    float3 nw = normalize(mul(float4(n, 0.0), world).xyz);                    // 法線をワールドへ (太陽の向きはワールド。 向きなので w = 0)
    float diffuse = saturate(dot(nw, LightDirection));                        // ランバート: 明るさ = cosθ = n·L (裏は 0。 L は太陽への向き)
    float3 albedo = float3(0.8, 0.85, 0.9);                                   // 形の色 (確かめる用の明るい灰色)
    float3 lit = albedo * (LightColor * diffuse + float3(0.22, 0.30, 0.42));  // 太陽の光 (色 × 強さ) + 空からの弱い光
    return float4(lit, 1.0);                                                  // 色 (光の強さのまま) と α (1 = 上書き)
}
