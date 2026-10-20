// =============================================================================
// IceShader6.hlsl — コップの氷 段階 6: 吸収と溶けかけの形
//
// 作る順に 1 段ずつ足していくシェーダーの、 途中の段。 完成形は IceShader10.hlsl (参考文献と全部の解説もそこ)。
// 前の段から足した行には 「★ 段階 N」 と書いてある。 前の段のファイルと見比べると、 足した所が分かる。
//
// 【段階】
//    0. 箱を描く
//    1. 形を探す (球追跡)
//    2. 法線と光 (不透明)
//    3. 透明にする (映り込みと透過)
//    4. 屈折 (歪み)
//    5. 全反射と後ろの面
//    6. 吸収と溶けかけの形  ← 今回
//    7. 白い芯 (内部散乱)
//    8. 気泡
//    9. 亀裂
//   10. コップ (完成)
//
// 【前の段から引き継ぐ物】
//   映り込み・屈折・全反射・後ろの面。 ここまでで、 氷の見た目の大部分ができている。
//
// 【今回足す物】
//   ・hash / noise: ランダムと値ノイズ
//   ・sdf: 溶けかけのゆるいでこぼこ (ノイズを足す)
//   ・rayMarchInside: 吸収 (ランベルト・ベールの法則) で thr を減らす
//
// 【仕組み】
//   値ノイズ: 空間をマスに分け、 マスの角に hash でランダムな値を置き、 間をなめらかに混ぜる (エルミート補間 + 三線形補間)。
//     hash は sin を使ったハッシュ: 内積で 1 つの数にし、 sin をかけて大きく引き伸ばし、 小数の部分だけを取る。
//   でこぼこ: 面の向きを場所ごとに少しずつ変えるので、 屈折した景色がゆらゆら波打つ。 細かく・大きくすると霜に見える。
//     SDF にノイズを足すと 「本当の距離より大きい」 所ができるので、 球追跡は 0.9 倍で控えめに進む (段階 1 から入れてある)。
//   吸収: 光が中を長さ L 進むと、 残る割合は T = exp(−σa L)。 1 歩ごとに exp(−σa × 歩幅) を掛ければ、 全部で exp(−σa L)。
//     赤を多く吸うと青っぽくなる (厚い氷が青い理由)。 小さな氷は通る長さが短いので、 吸収は弱くする (青くすると氷塊に見える)。
//
// 【確かめること】
//   ・屈折した床の模様が、 ゆるく波打つ
//   ・色はほとんど変わらない (C++ の setAbsorption を大きくすると青くなる。 試したら戻す)
//
// 【参考】
//   The Book of Shaders 第 10 章 ランダム・第 11 章 ノイズ (日本語)  https://thebookofshaders.com/10/?lan=jp  https://thebookofshaders.com/11/?lan=jp
//   PBR Book "Transmittance" (ランベルト・ベールの法則)  https://pbr-book.org/4ed/Volume_Scattering/Transmittance
// =============================================================================

#include "../../../src/engine/shaders/SceneConstants.hlsli"  // シーンの定数 (ViewProj・LightDirection・LightColor)

// 部品 1 つ分の定数。 C++ の SdfObject の ObjectConstants と同じ並び。 全部の段階で同じ (C++ は段階で変わらない)
cbuffer ObjectConstants : register(b1)
{
    float4x4 world;        // 部品の空間 → ワールドの行列 (転置済み。 mul(float4(p, 1), world) と書く)
    float4x4 invWorld;     // ワールド → 部品の空間の行列 (world の逆行列。 ピクセルシェーダーの最初に 1 回だけ使う)
    float3   cameraPos;    // カメラの位置 (ワールド)
    uint     shape;        // 形の番号: 0 = 氷 (角の丸い立方体)、 1 = コップ (段階 10 から)
    float3   absorption;   // 1 m あたりの吸収の強さ σa (赤・緑・青)。 段階 6 から使う
    float    ior;          // 屈折率 n (氷 1.31、 ガラス 1.5)。 段階 3 から使う
    float    coreDensity;  // 白い芯の気泡の濃さ σs (1/m)。 0 なら芯なし。 段階 7 から使う
    uint     thin;         // 1 = 薄い物 (コップ): 後ろに描いてある物に半透明で重ねる。 段階 10 から使う
    float2   objectPad;    // 16 バイトにそろえるための詰め物 (使わない)
};

// 環境マップ (周りの景色のキューブマップ)。 向きを渡すと、 その向きの景色の色 (光の強さ) が返る。 段階 3 から使う
TextureCube<float4> envMap : register(t0);
SamplerState envSampler : register(s0);  // 線形補間 (C++ の静的サンプラー)
static const float ICE_HALF = 0.13;           // 氷の半分の大きさ (m)。 1 辺 26 cm (場面は本物の約 8 倍)
static const float ICE_ROUND = 0.035;         // 氷の角の丸みの半径 (m)
static const float HIT_EPS = 0.0002;          // 表面まで、 これより近ければ 「当たった」 (m)。 0.2 mm
static const float EPS = 0.001;               // 表面から少し離す距離 (m)。 曲がった直後に、 同じ表面にまた当たらないように
static const float ICE_BUMP = 0.005;          // 溶けかけのでこぼこの大きさ (m)。 5 mm
static const float BUMP_FREQ = 8.0;           // でこぼこの細かさ (1 m あたりのマスの数)。 8 なら 12.5 cm ごと

// ---- ノイズ ----

// ランダム (sin のハッシュ): 点 → 0〜1 のでたらめな値。 同じ点からはいつも同じ値が出る
float hash(float3 p)
{
    return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453);  // 内積で 1 つの数に → sin → 大きく引き伸ばして小数の部分だけ
}

// 値ノイズ (−1〜1): 格子の角 8 つにランダムな値を置き、 間をなめらかに混ぜる
float noise(float3 p)
{
    float3 i = floor(p);                       // マスの角 (整数の点)
    float3 f = p - i;                          // マスの中の位置 (0〜1)
    float3 u = f * f * (3.0 - 2.0 * f);        // エルミート補間 u = 3f² − 2f³ (smoothstep と同じ曲線。 境目で傾きが 0)
    // 三線形補間: 角 8 つの値を、 x → y → z の順に混ぜる
    float x00 = lerp(hash(i), hash(i + float3(1.0, 0.0, 0.0)), u.x);                            // y = 0, z = 0 の辺
    float x10 = lerp(hash(i + float3(0.0, 1.0, 0.0)), hash(i + float3(1.0, 1.0, 0.0)), u.x);    // y = 1, z = 0 の辺
    float x01 = lerp(hash(i + float3(0.0, 0.0, 1.0)), hash(i + float3(1.0, 0.0, 1.0)), u.x);    // y = 0, z = 1 の辺
    float x11 = lerp(hash(i + float3(0.0, 1.0, 1.0)), hash(i + float3(1.0, 1.0, 1.0)), u.x);    // y = 1, z = 1 の辺
    return lerp(lerp(x00, x10, u.y), lerp(x01, x11, u.y), u.z) * 2.0 - 1.0;                    // y → z と混ぜて、 0〜1 を −1〜1 に
}

// ---- SDF (形): 点 p から形の表面までの符号付きの距離。 外が正、 表面が 0、 中が負 ----

// 角の丸い箱 (半分の大きさ b、 丸み r): 箱を r だけ小さくし、 表面を r だけふくらませる
float sdRoundBox(float3 p, float3 b, float r)
{
    float3 q = abs(p) - (b - r);                                          // 小さくした箱の各面から、 どれだけ外へ出ているか (負なら中)
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;  // 外: 出ている成分の長さ、 中: いちばん近い面まで。 − 丸み
}

// この部品の形 (部品の空間。 形の真ん中が原点)
float sdf(float3 p)
{
    float d = sdRoundBox(p, float3(ICE_HALF, ICE_HALF, ICE_HALF), ICE_ROUND);  // 氷: 角の丸い立方体
    d += ICE_BUMP * noise(p * BUMP_FREQ);  // ★ 段階 6: 溶けかけのゆるいでこぼこ (細かく・大きくすると霜に見える)
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

// 中から出口まで進む: 中では −SDF が 「いちばん近い表面までの距離」 なので、 その分進めば表面を越えない。
// 戻り値: 出口 (後ろの面) までの距離。 col / thr: 進みながら足す光と、 残る割合 (段階 6 から使う)
float rayMarchInside(float3 ro, float3 rd, inout float3 col, inout float3 thr)
{
    float t = 0.0;                                    // 進んだ距離
    [loop]
    for (int i = 0; i < 128; ++i) {                   // 歩数の上限
        float3 p = ro + rd * t;                       // 今いる点
        float depth = -sdf(p);                        // 表面からの深さ (中なので −SDF)
        if (depth < HIT_EPS) break;                   // 出口に着いた
        float s = clamp(depth, HIT_EPS, 0.02);        // 1 歩: 表面までの深さ。 2 cm まで (中の濃さの変わり方を読み飛ばさない)
        thr *= exp(-absorption * s);                  // ★ 段階 6: 吸収。 透過率 T = exp(−σa × 距離) (ランベルト・ベールの法則)
        t += s;
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

    [loop]
    for (int entry = 0; entry < 2; ++entry) {  // 同じ形に入るのは 2 回まで (へこんだ所や、 コップの手前の壁 → 奥の壁)
        float3 n = calcNormal(p);              // 表面の法線

        // 2. 表面: 映り込みを足し、 残りは屈折して中へ
        float F = fresnelSchlick(dot(-rd, n));                                // 映り込む割合 (フレネル)
        col += thr * F * envMap.SampleLevel(envSampler, mul(float4(reflect(rd, n), 0.0), world).xyz, 0).rgb;  // 反射の向き r = d − 2(d·n)n の景色
        thr *= 1.0 - F;                                                       // 残りの割合が中へ入る
        rd = refract(rd, n, 1.0 / ior);                                       // ★ 段階 4: 屈折。 スネルの法則 (空気 → 中。 屈折率の比 1 / n)
        p -= n * EPS;                                                         // 少し中へ入れてから進む

        bool escaped = false;                                                 // 外へ出られたか
        // 3〜4. 中を出口まで進む。 出られない角度なら全反射して、 また進む
        [loop]
        for (int k = 0; k < 4; ++k) {                                         // 全反射は 4 回まで
            p += rd * rayMarchInside(p, rd, col, thr);                        // 3. 出口 (後ろの面) まで進む
            n = calcNormal(p);                                                // 出口の法線
            float3 outDir = refract(rd, -n, ior);                             // スネルの法則 (中 → 空気。 比 n / 1。 法線は光線と向かい合う −n)
            if (dot(outDir, outDir) < 1e-6) {                                 // ★ 段階 5: 解が無い (refract が (0, 0, 0)) = 全反射 (sinθ > 1 / n)
                rd = reflect(rd, n);                                          // 中で跳ね返る
                p -= n * EPS;                                                 // 少し中へ戻す
                continue;                                                     // また出口を探す
            }
            F = fresnelSchlick(dot(outDir, n));                               // 出口で映り込む割合
            // ★ 段階 5: 後ろの面の映り込み。 中へ跳ね返る分 F を、 跳ね返った向きの景色で近似して足す
            col += thr * F * envMap.SampleLevel(envSampler, mul(float4(reflect(rd, n), 0.0), world).xyz, 0).rgb;
            thr *= 1.0 - F;                                                   // 残りは外へ
            rd = outDir;                                                      // 曲がって外へ
            p += n * EPS;                                                     // 少し外へ出す
            escaped = true;
            break;
        }

        // 5. 外へ出た光が、 同じ形にまた当たるか。 当たらなければ終わり
        float tAgain;                                                         // また当たるまでの距離
        if (!escaped || entry == 1 || !rayMarch(p, rd, 2.0, tAgain)) break;   // 出られなかった / 2 回目 / もう当たらない
        p += rd * tAgain;                                                     // また当たった表面へ
    }

    // 6. 景色
    col += thr * envMap.SampleLevel(envSampler, mul(float4(rd, 0.0), world).xyz, 0).rgb;  // 出た向きの空と床に、 残った割合を掛けて足す
    return float4(col, 1.0);                                                  // 色 (光の強さのまま) と α (1 = 上書き)
}
