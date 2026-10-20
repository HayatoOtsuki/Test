// =============================================================================
// IceShader3.hlsl — コップの氷 段階 3: 透明にする (映り込みと透過)
//
// 作る順に 1 段ずつ足していくシェーダーの、 途中の段。 完成形は IceShader10.hlsl (参考文献と全部の解説もそこ)。
// 前の段から足した行には 「★ 段階 N」 と書いてある。 前の段のファイルと見比べると、 足した所が分かる。
//
// 【段階】
//    0. 箱を描く
//    1. 形を探す (球追跡)
//    2. 法線と光 (不透明)
//    3. 透明にする (映り込みと透過)  ← 今回
//    4. 屈折 (歪み)
//    5. 全反射と後ろの面
//    6. 吸収と溶けかけの形
//    7. 白い芯 (内部散乱)
//    8. 気泡
//    9. 亀裂
//   10. コップ (完成)
//
// 【前の段から引き継ぐ物】
//   形・球追跡・法線・光線・深度。 段階 2 のランバートの光は使わなくなる。
//
// 【今回足す物】
//   ・fresnelSchlick: 映り込む割合 (フレネル、 Schlick の近似)
//   ・envMap (t0) / envSampler (s0): 環境マップ (周りの景色のキューブマップ)。 向きを渡すと、 その向きの景色の色が返る
//   ・ピクセルシェーダー: 表面で映り込みを足し、 残りは 「曲げずに」 まっすぐ透かした景色を足す
//   ・col (足していく光) と thr (まだ届く割合) の 2 つで光を運ぶ形にする。 ここから先は全部この形
//
// 【仕組み】
//   フレネル: 光が表面に当たると、 一部は映り込み、 残りが中へ入る。 映り込む割合 F は、 正面ではわずか (氷 1.8%)、
//     浅い角度 (縁) ほど 1 に近づく。 F = F0 + (1 − F0)(1 − cosθ)⁵、 F0 = ((n − 1) / (n + 1))²。
//   反射の向き: r = d − 2(d·n)n (HLSL の reflect)。 その向きの景色が映り込みの色。
//   環境マップ: 周りの景色を 6 面の画像 (キューブマップ) にしておき、 向きで読む (envMap.SampleLevel(envSampler, 向き, 0))。
//     向きはワールドの向きなので、 部品の空間の向きを mul(float4(向き, 0), world) で直してから読む。
//     サンプルでは背景と同じ式で C++ が作る (EnvironmentImage)。 自分のエンジンでは、 スカイボックスのキューブマップを渡せばよい。
//   col / thr: 目に届く光を col に足し、 まだ先へ届く割合を thr に掛けて減らしていく。 映り込みを足したら thr *= 1 − F。
//
// 【確かめること】
//   ・氷が透明になる。 正面は向こうの床がそのまま見え、 縁ほど空が白く映り込む
//   ・これが 「屈折の無い透明」。 次の段で屈折を入れると、 急に氷らしくなる
//
// 【参考】
//   LearnOpenGL "PBR Theory" (Fresnel equation)  https://learnopengl.com/PBR/Theory
//   LearnOpenGL "Cubemaps" (Environment mapping・Reflection)  https://learnopengl.com/Advanced-OpenGL/Cubemaps
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

    // ★ 段階 3: 透明にする。 表面で映り込み (フレネル) を足し、 残りは曲げずにまっすぐ透かす
    float3 n = calcNormal(p);                                                 // 表面の法線
    float F = fresnelSchlick(dot(-rd, n));                                    // 映り込む割合 (フレネル)
    col += thr * F * envMap.SampleLevel(envSampler, mul(float4(reflect(rd, n), 0.0), world).xyz, 0).rgb;  // 反射の向き r = d − 2(d·n)n の景色
    thr *= 1.0 - F;                                                           // 残りの割合が通り抜ける
    col += thr * envMap.SampleLevel(envSampler, mul(float4(rd, 0.0), world).xyz, 0).rgb;  // まっすぐ透かした景色 (屈折なし)
    return float4(col, 1.0);                                                  // 色 (光の強さのまま) と α (1 = 上書き)
}
