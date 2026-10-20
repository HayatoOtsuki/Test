// =============================================================================
// IceShader5.hlsl — 氷 段階 5: 全反射と後ろの面
//
// 作る順に 1 段ずつ足していくシェーダーの、 途中の段。 完成形は IceShader13.hlsl (参考文献と全部の解説もそこ)。
// 前の段から足した行には 「★ 段階 N」 と書いてある。 前の段のファイルと見比べると、 足した所が分かる。
//
// 【段階】
//    0. 箱を描く
//    1. 形を探す (球追跡)
//    2. 法線と光 (不透明)
//    3. 透明にする (映り込みと透過)
//    4. 屈折 (歪み)
//    5. 全反射と後ろの面  ← 今回
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
//   入口と出口の屈折、 中を進む rayMarchInside、 同じ形にまた入るループ。
//
// 【今回足す物】
//   ・全反射: 出口で refract の解が無ければ、 外へ出さずに中で跳ね返り、 また出口を探す (8 回まで)
//   ・後ろの面の映り込み: 出口で中へ跳ね返る分 F を、 跳ね返った向きの景色で近似して足す
//
// 【仕組み】
//   全反射: 氷 → 空気では n1 = 1.31 > n2 = 1 なので、 sinθ2 = 1.31 sinθ1 が 1 を超えると、 スネルの法則を満たす向きが無い。
//     臨界角 θc = arcsin(1 / n) = 49.8° (氷)。 HLSL の refract は、 このとき (0, 0, 0) を返す。 長さ 0 なら全反射と判断する。
//     氷やガラスがきらきら見えるのは、 中から出ようとする光の多くが出られないため。 面ごとのまだらや、 縁の銀色はこれ。
//   後ろの面: 透明なので、 表の面の向こうに後ろの面 (出口) が見える。 出口で中へ跳ね返る光を足すと、 後ろの面が光って見える。
//     本当は跳ね返った光をもう一度追うが、 光を 2 本に分けると重いので近似にする (このサンプルで決めた近似)。
//
// 【確かめること】
//   ・面ごとに明るい所と暗い所ができ、 縁が銀色に光る
//   ・斜めに見た後ろの面が光る
//
// 【参考】
//   Microsoft "refract" (全反射で (0, 0, 0))  https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-refract
//   PBR Book "Specular Reflection and Transmission" (全反射)  https://pbr-book.org/4ed/Reflection_Models/Specular_Reflection_and_Transmission
//
// 【コードの引用元】
//   全反射の判定 (refract の長さが 0): Microsoft の refract の説明 (全反射で (0, 0, 0) を返す) から
//   8 回のループ・後ろの面の近似: 自分で書いた (このサンプルで決めた近似)
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

// 環境マップ (周りの景色のキューブマップ)。 向きを渡すと、 その向きの景色の色 (光の強さ) が返る。 段階 3 から使う
TextureCube<float4> envMap : register(t0);
SamplerState envSampler : register(s0);  // 線形補間 (C++ の静的サンプラー)
static const float ICE_HALF = 0.13;           // 氷の半分の大きさ (m)。 1 辺 26 cm (場面は本物の約 8 倍)
static const float ICE_ROUND = 0.035;         // 氷の角の丸みの半径 (m)
static const float HIT_EPS = 0.0002;          // 表面まで、 これより近ければ 「当たった」 (m)。 0.2 mm
static const float EPS = 0.001;               // 表面から少し離す距離 (m)。 曲がった直後に、 同じ表面にまた当たらないように

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

// ---- 光 ----

// フレネル (Schlick の近似): F = F0 + (1 − F0)(1 − cosθ)⁵。 正面ではわずか、 浅い角度ほど強く映り込む
float fresnelSchlick(float cosTheta)
{
    float f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);                // 正面の反射率 F0 = ((n − 1) / (n + 1))² (氷 0.018、 ガラス 0.04)
    return f0 + (1.0 - f0) * pow(1.0 - saturate(cosTheta), 5.0);  // cosθ = 光線と法線のなす角の cos
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

// 中から出口まで進む。 1 歩は −SDF (いちばん近い表面までの距離) だが、 2 mm〜2 cm に収める。
// 「表面に近づいたら出口」 とはせず、 SDF が正 (外) になるまで進んでから、 2 分探索で出口を詰める。
//   −SDF だけ進む形だと、 斜めの光線が面とほぼ平行に表面のすぐ近くを進むとき、 歩幅が小さくなって歩数を使い切ったり、
//   出ていないのに 「出口に着いた」 と判断したりする (斜めから見ると、 中の面が重なって見える)。
// 戻り値: 出口 (後ろの面) までの距離。 col / thr: 進みながら足す光と、 残る割合 (段階 6 から使う)
float rayMarchInside(float3 ro, float3 rd, inout float3 col, inout float3 thr)
{
    float t = 0.0;                                    // 進んだ距離
    float tIn = 0.0;                                  // 中だと分かっている、 いちばん遠い距離 (2 分探索に使う)
    [loop]
    for (int i = 0; i < 128; ++i) {                   // 歩数の上限
        float3 p = ro + rd * t;                       // 今いる点
        float depth = -sdf(p);                        // 表面からの深さ (中なので −SDF)
        if (depth < 0.0) break;                       // 外へ出た (出口を越えた)
        tIn = t;                                      // ここはまだ中
        float s = clamp(depth, 0.002, 0.02);          // 1 歩: 表面までの深さ。 2 mm〜2 cm (小さすぎると歩数を使い切り、 大きいと中の濃さを読み飛ばす)
        t += s;
    }
    // 2 分探索: tIn (中) と t (外) の真ん中が外なら t を、 中なら tIn を縮める。 6 回で幅は 1/64 (2 cm なら 0.3 mm)
    [loop]
    for (int j = 0; j < 6; ++j) {
        float m = 0.5 * (tIn + t);
        if (sdf(ro + rd * m) > 0.0) t = m;
        else tIn = m;
    }
    return t;
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

    float3 col = float3(0.0, 0.0, 0.0);  // 目に届く光 (足していく)
    float3 thr = float3(1.0, 1.0, 1.0);  // まだ届く割合 (掛けて減らしていく。 透過率)

    // 2. 表面: 映り込みを足し、 残りは屈折して中へ
    float3 n = calcNormal(p);                                                 // 表面の法線
    float F = fresnelSchlick(dot(-rd, n));                                    // 映り込む割合 (フレネル)
    col += thr * F * envMap.SampleLevel(envSampler, mul(float4(reflect(rd, n), 0.0), world).xyz, 0).rgb;  // 反射の向き r = d − 2(d·n)n の景色
    thr *= 1.0 - F;                                                           // 残りの割合が中へ入る
    rd = refract(rd, n, 1.0 / ior);                                           // ★ 段階 4: 屈折。 スネルの法則 (空気 → 中。 屈折率の比 1 / n)
    p -= n * EPS;                                                             // 少し中へ入れてから進む

    // 3〜4. 中を出口まで進む。 出られない角度なら全反射して、 また進む
    [loop]
    for (int k = 0; k < 8; ++k) {                                             // 全反射は 8 回まで (角の近くでは何度も跳ね返る)
        p += rd * rayMarchInside(p, rd, col, thr);                            // 3. 出口 (後ろの面) まで進む
        n = calcNormal(p);                                                    // 出口の法線
        float3 outDir = refract(rd, -n, ior);                                 // スネルの法則 (中 → 空気。 比 n / 1。 法線は光線と向かい合う −n)
        if (dot(outDir, outDir) < 1e-6) {                                     // ★ 段階 5: 解が無い (refract が (0, 0, 0)) = 全反射 (sinθ > 1 / n)
            rd = reflect(rd, n);                                              // 中で跳ね返る
            p -= n * EPS;                                                     // 少し中へ戻す
            continue;                                                         // また出口を探す
        }
        F = fresnelSchlick(dot(outDir, n));                                   // 出口で映り込む割合
        // ★ 段階 5: 後ろの面の映り込み。 中へ跳ね返る分 F を、 跳ね返った向きの景色で近似して足す
        col += thr * F * envMap.SampleLevel(envSampler, mul(float4(reflect(rd, n), 0.0), world).xyz, 0).rgb;
        thr *= 1.0 - F;                                                       // 残りは外へ
        rd = outDir;                                                          // 曲がって外へ
        break;
    }

    // 5. 景色: 出た向きの景色に、 残った割合を掛けて足す
    col += thr * envMap.SampleLevel(envSampler, mul(float4(rd, 0.0), world).xyz, 0).rgb;
    return float4(col, 1.0);                                                  // 色 (光の強さのまま) と α (1 = 上書き)
}
