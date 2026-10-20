// =============================================================================
// IceShader4.hlsl — コップの氷 段階 4: 屈折 (歪み)
//
// 作る順に 1 段ずつ足していくシェーダーの、 途中の段。 完成形は IceShader10.hlsl (参考文献と全部の解説もそこ)。
// 前の段から足した行には 「★ 段階 N」 と書いてある。 前の段のファイルと見比べると、 足した所が分かる。
//
// 【段階】
//    0. 箱を描く
//    1. 形を探す (球追跡)
//    2. 法線と光 (不透明)
//    3. 透明にする (映り込みと透過)
//    4. 屈折 (歪み)  ← 今回
//    5. 全反射と後ろの面
//    6. 吸収と溶けかけの形
//    7. 白い芯 (内部散乱)
//    8. 気泡
//    9. 亀裂
//   10. コップ (完成)
//
// 【前の段から引き継ぐ物】
//   形・球追跡・法線・フレネル・col / thr。 段階 3 の 「まっすぐ透かす」 は、 屈折して中を通る形に置き換わる。
//
// 【今回足す物】
//   ・入口で屈折して中へ (refract、 屈折率の比 1 / n)
//   ・rayMarchInside: 中を出口 (後ろの面) まで進む
//   ・出口で屈折して外へ (refract、 比 n / 1、 法線は −n)
//   ・外へ出た光が、 同じ形にまた当たるかを調べるループ (entry)。 氷ではほとんど 1 回で終わる (コップの段で効く)
//
// 【仕組み】
//   スネルの法則: n1 sinθ1 = n2 sinθ2。 屈折率の違う物の境目で光は曲がる。 入るときは法線に近づき、 出るときは離れる。
//     HLSL の refract(入る向き, 光線と向かい合う法線, 屈折率の比 n1 / n2) が曲がった向きを返す。
//   中を進む: 中では SDF が負で、 −SDF が 「いちばん近い表面までの距離」。 外と同じく、 その分進めば表面を越えない。
//   歪み: 曲がった向きで景色を読むと、 向こうがずれて見える。 面の向き・中を通る長さ・出口の面の向きで、 ずれ方が勝手に変わる。
//   この段では全反射をまだ扱わない (出られないはずの光も、 まっすぐ出してしまう)。
//
// 【確かめること】
//   ・氷を通した床の模様が、 曲がって見える (ここで急に氷らしくなる)
//   ・角の近くで、 景色が大きく引き伸ばされる
//
// 【参考】
//   LearnOpenGL "Cubemaps" (Refraction)  https://learnopengl.com/Advanced-OpenGL/Cubemaps
//   PBR Book "Specular Reflection and Transmission" (スネルの法則)  https://pbr-book.org/4ed/Reflection_Models/Specular_Reflection_and_Transmission
// =============================================================================

#include "../../../src/engine/shaders/Environment.hlsli"  // シーンの定数 (ViewProj・太陽の向きと色) と景色 (Environment)

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

// この部品の形 (部品の空間。 形の真ん中が原点)
float sdf(float3 p)
{
    float d = sdRoundBox(p, float3(ICE_HALF, ICE_HALF, ICE_HALF), ICE_ROUND);  // 氷: 角の丸い立方体
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
        col += thr * F * Environment(mul(float4(p, 1.0), world).xyz, mul(float4(reflect(rd, n), 0.0), world).xyz);  // 反射の向き r = d − 2(d·n)n の景色
        thr *= 1.0 - F;                                                       // 残りの割合が中へ入る
        rd = refract(rd, n, 1.0 / ior);                                       // ★ 段階 4: 屈折。 スネルの法則 (空気 → 中。 屈折率の比 1 / n)
        p -= n * EPS;                                                         // 少し中へ入れてから進む

        bool escaped = false;                                                 // 外へ出られたか
        // 3. 中を出口まで進む → 4. 出口で屈折して外へ
        p += rd * rayMarchInside(p, rd, col, thr);                            // 出口 (後ろの面) まで進む
        n = calcNormal(p);                                                    // 出口の法線
        float3 outDir = refract(rd, -n, ior);                                 // ★ 段階 4: スネルの法則 (中 → 空気。 比 n / 1。 法線は光線と向かい合う −n)
        if (dot(outDir, outDir) < 1e-6) outDir = rd;                          // 段階 4: 全反射はまだ扱わない (出られない光もまっすぐ出してしまう)
        thr *= 1.0 - fresnelSchlick(dot(outDir, n));                          // 出口でも一部は映り込んで中へ戻る (段階 4 では捨てる)
        rd = outDir;                                                          // 曲がって外へ
        p += n * EPS;                                                         // 少し外へ出す
        escaped = true;

        // 5. 外へ出た光が、 同じ形にまた当たるか。 当たらなければ終わり
        float tAgain;                                                         // また当たるまでの距離
        if (!escaped || entry == 1 || !rayMarch(p, rd, 2.0, tAgain)) break;   // 出られなかった / 2 回目 / もう当たらない
        p += rd * tAgain;                                                     // また当たった表面へ
    }

    // 6. 景色
    col += thr * Environment(mul(float4(p, 1.0), world).xyz, mul(float4(rd, 0.0), world).xyz);  // 出た向きの空と床に、 残った割合を掛けて足す
    return float4(col, 1.0);                                                  // 色 (光の強さのまま) と α (1 = 上書き)
}
