// =============================================================================
// IceShader10.hlsl — コップの氷 (完成形)
//
// SdfObject (RenderEntity) 1 つが、 このシェーダーで形を 1 つ描く。 サンプルは氷 3 つとコップ 1 つを置く。
// 部品は形を囲む箱の裏面を描き、 箱が映る画素で光線を追う。
//
// 【描く流れ】 (ピクセルシェーダー PSMain)
//   0. 最初に 1 回だけ、 カメラを部品の空間へ (invWorld)。 ここから先は全部、 部品の空間で計算する
//   1. rayMarch       : 外から表面を探す (球追跡)。 当たらなければ discard。 当たった点の深度を出す
//   2. 表面           : 映り込み (フレネル) を足し、 残りは屈折 (スネルの法則) して中へ
//   3. rayMarchInside : 中を出口まで進む。 吸収 (ランベルト・ベール)・白い芯 (内部散乱)・気泡・亀裂
//   4. 出口           : 屈折して外へ。 出られない角度なら全反射して 3 へ。 後ろの面の映り込み
//   5. 外へ出た光が同じ形にまた当たるなら 2 へ (コップの手前の壁 → 奥の壁)
//   6. 景色           : 氷は、 出た向きの景色を環境マップ (envMap) から読む。 コップ (薄い物) は後ろに描いてある物に半透明で重ねる
//   返す値は float4(色, α)。 色は光の強さのまま (表示用の変換は、 エンジンが最後に 1 回だけする)
//   エンジンの関数は使わない。 使うのは定数バッファ (ViewProj・LightDirection・LightColor と、 部品の定数) と環境マップ (t0) だけ
//
// 【式と仕組みの一覧】 (詳しくは samples/06_glass_ice/README.md)
//   SDF                : 点から形の表面までの符号付きの距離。 外が正、 表面が 0、 中が負
//   角の丸い箱         : 箱を丸みの分小さくし、 表面を丸みの分ふくらませる
//   殻 (onion)         : |φ| − 厚さの半分。 共通部分は max(φ1, φ2)
//   法線 (中心差分)    : n = ∇φ / |∇φ|。 各軸で少し前と後ろの差
//   球追跡             : φ だけ進めば表面を越えない (中では −φ)
//   フレネル (Schlick) : F = F0 + (1 − F0)(1 − cosθ)⁵、 F0 = ((n − 1) / (n + 1))²
//   反射の向き         : r = d − 2(d·n)n
//   スネルの法則       : n1 sinθ1 = n2 sinθ2 (refract)
//   全反射             : sinθ > 1 / n で解が無い。 臨界角 = arcsin(1 / n) (氷 49.8°、 ガラス 41.8°)
//   ランベルト・ベール : 透過率 T = exp(−σ × 距離)
//   内部散乱 (簡単版)  : 1 歩で当たる割合 a = 1 − exp(−σs × 歩幅)。 col += thr × a × 届く光、 thr *= 1 − a
//   ランダム・値ノイズ : sin のハッシュ、 エルミート補間 3f² − 2f³、 三線形補間
//   光線と球の交点     : t = −b ± √(b² − c)、 b = oc·d、 c = |oc|² − r²
//   光線と平面の交点   : t = (c − p)·n / (d·n)
//
// 【このサンプルで決めた近似と値】 (文献に無い所。 説明するときは、 この区別をそのまま言う)
//   ・後ろの面の映り込み: 出口で中へ跳ね返る分を、 跳ね返った向きの景色で近似する (本当はもう一度追う)
//   ・全反射は 4 回まで。 残りは今の向きの景色で近似
//   ・内部散乱: 一回散乱だけ、 向きの偏り無し、 届く光は決まった白 (CORE_LIGHT)
//   ・気泡と亀裂: 中を通り抜ける光の曲がりは無視。 亀裂の白い散らばりの割合 (CRACK_WHITE)
//   ・環境マップは、 コップの真ん中から見た景色 (床の模様は、 その点から見た物。 氷の場所ごとの違いは無視)
//   ・コップ: 壁で光が曲がらないとみなし、 後ろの絵に重ねる (float4(映り込み, 1 − 透過率) を返してブレンド)
//   ・形の大きさ・でこぼこ・吸収・芯・気泡・亀裂の値は、 見た目で決めた
//
// 【参考文献】 (どれも式と考え方を確かめるために読んだ。 コードは写していない。 利用条件は docs/guide/references.md)
//   ・SDF の形 (角の丸い箱・円柱・onion・共通部分): Inigo Quilez "distance functions"
//       https://iquilezles.org/articles/distfunctions  (記事のコード片は MIT ライセンス)
//   ・SDF の法線 (中心差分): Inigo Quilez "normals for an SDF"  https://iquilezles.org/articles/normalsSDF
//   ・球追跡: Inigo Quilez "raymarching SDFs"  https://iquilezles.org/articles/raymarchingdf
//       原典: J. C. Hart, "Sphere Tracing", The Visual Computer 12, 1996  https://doi.org/10.1007/s003710050084
//   ・光線と球・平面の交点: Inigo Quilez "intersectors"  https://iquilezles.org/articles/intersectors
//   ・ランダム (sin のハッシュ)・値ノイズ・マスごとに点を置く: The Book of Shaders 第 10・11・12 章 (日本語あり)
//       https://thebookofshaders.com/10/?lan=jp  https://thebookofshaders.com/11/?lan=jp  https://thebookofshaders.com/12/?lan=jp
//   ・反射と屈折の向き (reflect / refract)、 環境マップ (キューブマップを向きで読む): LearnOpenGL "Cubemaps"
//       https://learnopengl.com/Advanced-OpenGL/Cubemaps
//       HLSL の refract (全反射の角度では (0, 0, 0) を返す):
//       https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-refract
//   ・フレネル (Schlick の近似): LearnOpenGL "PBR Theory"  https://learnopengl.com/PBR/Theory
//       原典: C. Schlick, Computer Graphics Forum 13(3), 1994  https://doi.org/10.1111/1467-8659.1330233
//   ・スネルの法則・全反射・F0: PBR Book 第 4 版 (全文が無料で公開)
//       https://pbr-book.org/4ed/Reflection_Models/Specular_Reflection_and_Transmission
//   ・ランベルト・ベールの法則 (透過率)・内部散乱 (放射伝達の式): PBR Book 第 4 版
//       https://pbr-book.org/4ed/Volume_Scattering/Transmittance
//       https://pbr-book.org/4ed/Volume_Scattering/Volume_Scattering_Processes
//   ・全反射 (密な物 → 疎な物): Wikipedia "Total internal reflection"  https://en.wikipedia.org/wiki/Total_internal_reflection
//   ・冷凍庫の氷が白く濁る理由: Wikipedia "Ice cube" の Clear and cloudy  https://en.wikipedia.org/wiki/Ice_cube
//   ・氷の屈折率 1.31: S. G. Warren, R. E. Brandt, J. Geophys. Res. 113, 2008  https://doi.org/10.1029/2007JD009744
//   ・SV_Depth / discard: Microsoft の HLSL の説明
//       https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics
//       https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/discard--sm4---asm-
//
// 【段階】 IceShader0.hlsl から順に 1 段ずつ足して、 このファイルになる。 各段のヘッダーに、 足した物と確かめることがある。
//    0. 箱を描く
//    1. 形を探す (球追跡)
//    2. 法線と光 (不透明)
//    3. 透明にする (映り込みと透過)
//    4. 屈折 (歪み)
//    5. 全反射と後ろの面
//    6. 吸収と溶けかけの形
//    7. 白い芯 (内部散乱)
//    8. 気泡
//    9. 亀裂
//   10. コップ (完成)  ← 今回
//
// 【前の段から引き継ぐ物】
//   氷の全部 (でこぼこ・屈折・全反射・後ろの面・吸収・白い芯・気泡・亀裂)。
//
// 【今回足す物】
//   ・sdCappedCylinder と sdf の shape == 1: コップの形 (円柱を殻にして、 上を切って口を開ける)
//   ・ピクセルシェーダーの最後: 薄い物 (thin) は、 後ろに描いてある物に半透明で重ねる
//   ・C++: コップの部品を描く (氷の後に描く)
//
// 【仕組み】
//   コップの形: 円柱の SDF の絶対値から厚さの半分を引くと、 壁と底だけの殻になる (onion: |d| − t)。
//     「その高さより下」 の SDF と共通部分 (max) を取ると、 上のふたが切れて口が開く。
//   同じ形にまた入るループ (段階 4 から) で、 手前の壁 → 奥の壁 と 2 回通る。
//   重ね方: 壁が薄いので、 通り抜けた光はほとんど曲がらない。 だから通った先の景色は自分で計算せず、 先に描いた氷と床を使う。
//     ピクセルシェーダーは float4(映り込みの光, 通らなかった割合) を返す。 ブレンドが 「出した色 + 後ろの色 × (1 − α)」 なので、
//     結果 = 映り込み + 後ろの色 × 透過率。 シーンは明るさのまま描いているので、 光の足し算として正しく混ざる。
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
static const float3 CORE_LIGHT = float3(0.85, 0.9, 0.95);  // 気泡に当たって散らばる光の色 (届く光を決まった白とする近似)
static const float BUBBLE_CELL = 0.035;       // 気泡を置くマスの大きさ (m)。 マス 1 つに気泡は 0 か 1 個
static const float BUBBLE_R_MIN = 0.0015;     // 気泡の半径の最小 (m)
static const float BUBBLE_R_MAX = 0.004;      // 気泡の半径の最大 (m)。 マスの面までの余白 (マスの 30%) より小さくする
static const float CRACK_R = 0.08;            // 亀裂 (円盤) の半径 (m)。 ふちはノイズでゆらす
static const float CRACK_WHITE = 0.08;        // 亀裂を通る光のうち、 割れ目の細かいひびで白く散らばる割合
static const float CUP_R = 0.36;              // コップの半径 (壁の真ん中まで, m)
static const float CUP_H = 0.40;              // コップの高さの半分 (m)
static const float CUP_T = 0.006;             // コップの壁の厚さの半分 (m)。 壁と底の厚さは 1.2 cm

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

// 3 つのでたらめな値 (0〜1)。 点を少しずつずらして hash を 3 回呼ぶ
float3 hash3(float3 p)
{
    return float3(hash(p), hash(p + float3(31.7, 11.3, 57.1)), hash(p + float3(71.9, 43.3, 23.7)));
}

// ---- SDF (形): 点 p から形の表面までの符号付きの距離。 外が正、 表面が 0、 中が負 ----

// 角の丸い箱 (半分の大きさ b、 丸み r): 箱を r だけ小さくし、 表面を r だけふくらませる
float sdRoundBox(float3 p, float3 b, float r)
{
    float3 q = abs(p) - (b - r);                                          // 小さくした箱の各面から、 どれだけ外へ出ているか (負なら中)
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;  // 外: 出ている成分の長さ、 中: いちばん近い面まで。 − 丸み
}

// 円柱 (半径 r、 高さの半分 h): 中心軸からの距離と縦の位置を、 2 次元の箱として測る
float sdCappedCylinder(float3 p, float r, float h)
{
    float2 d = abs(float2(length(p.xz), p.y)) - float2(r, h);  // 横 (中心軸から) と縦の、 面からのはみ出し
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));     // 2 次元の箱の距離関数
}

// この部品の形 (部品の空間。 形の真ん中が原点)
float sdf(float3 p)
{
    if (shape == 1) {
        // ★ 段階 10: コップ。 円柱を殻にし (onion: |d| − 厚さの半分)、 上を切って口を開ける (共通部分: max)
        float shell = abs(sdCappedCylinder(p, CUP_R, CUP_H)) - CUP_T;  // 壁と底だけの、 ふたのある筒
        return max(shell, p.y - (CUP_H - CUP_T));                      // 「この高さより下」 と共通部分 = ふたを切る
    }
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

// ---- 気泡 ----

// 点 p のマスに置いた気泡の表面までの距離 (球の距離関数)。 気泡が無いマスは大きな値を返す。
// center / radius: 気泡の中心と半径。 cellStep: 光線 rd の向きに進んで、 このマスを出るまでの距離
// (それを越えて進むと、 となりのマスの気泡を確かめずに通り過ぎてしまう)
float nearestBubble(float3 p, float3 rd, out float3 center, out float radius, out float cellStep)
{
    float3 seed = mul(float4(0.0, 0.0, 0.0, 1.0), world).xyz;     // 部品の位置 (ワールド)。 氷ごとに気泡の並びを変える
    float3 cell = floor(p / BUBBLE_CELL);                           // マスの番号 (整数)
    float3 r = hash3(cell + seed * 13.0);                           // このマスのでたらめな値 3 つ (0〜1)
    // マスを出るまでの距離: 各軸で、 光線が進む向きにある面 (rd が正なら大きい側、 負なら小さい側) までの t を求め、 いちばん小さい物
    float3 boundary = (cell + step(0.0, rd)) * BUBBLE_CELL;         // 光線が進む向きにある、 各軸のマスの面
    float3 safeRd = (step(0.0, rd) * 2.0 - 1.0) * max(abs(rd), 1e-6);  // 0 で割らないよう、 符号を保ったまま小さい値で止める
    float3 tFace = (boundary - p) / safeRd;                          // 各軸の面に届く距離 t = (面 − p) / rd
    cellStep = min(tFace.x, min(tFace.y, tFace.z)) + 0.0005;         // マスを出る距離 + 0.5 mm (面を少し越えて、 となりのマスへ)
    center = (cell + 0.5 + (r - 0.5) * 0.4) * BUBBLE_CELL;          // 気泡の中心: マスの真ん中 ± 20% (面から 30% は離す)
    radius = lerp(BUBBLE_R_MIN, BUBBLE_R_MAX, frac(r.x * 7.31));     // 半径: 1.5〜4 mm
    float centerDepth = -sdf(center);                                // 気泡の中心の、 氷の表面からの深さ
    float rate = 0.05 + 0.25 * smoothstep(0.03, 0.10, centerDepth);  // 気泡がある確率: 中心が深い (芯に近い) マスほど多い (5〜30%)
    if (frac(r.y * 3.17 + r.z * 5.11) > rate || centerDepth < radius) return 1e3;  // このマスに気泡は無い (氷からはみ出す物も置かない)
    return length(p - center) - radius;                              // 球の距離関数: 中心までの距離 − 半径
}

// ---- 亀裂: 氷の中の 1 枚の割れ目 (薄い空気の層)。 平面の上の、 ふちがぎざぎざの円盤として表す ----

// 亀裂の面の法線を返し、 center に円盤の中心を入れる
float3 crackPlane(out float3 center)
{
    float3 seed = mul(float4(0.0, 0.0, 0.0, 1.0), world).xyz;  // 部品の位置 (ワールド)。 氷ごとに亀裂の向きを変える
    float3 r = hash3(seed * 7.0);                                // でたらめな値 3 つ (0〜1)
    center = (r - 0.5) * 0.04;                                   // 円盤の中心: 氷の真ん中 ± 2 cm
    return normalize(r * 2.0 - 1.0 + float3(0.0, 0.0, 0.001));   // 面の向き: でたらめ (長さ 0 にならないよう少し足す)
}

// 面の上の点 q (円盤の中心からの位置) が、 ぎざぎざのふちの内側か
bool insideCrack(float3 q)
{
    float edge = CRACK_R * (0.7 + 0.5 * noise(q * 30.0));  // ふちの半径を、 場所ごとにノイズでゆらす (ぎざぎざ)
    return length(q) < edge;
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
    float3 crackC;                                    // ★ 段階 9: 亀裂の円盤の中心
    float3 crackN = crackPlane(crackC);               // ★ 段階 9: 亀裂の面の法線
    float t = 0.0;                                    // 進んだ距離
    [loop]
    for (int i = 0; i < 128; ++i) {                   // 歩数の上限
        float3 p = ro + rd * t;                       // 今いる点
        float depth = -sdf(p);                        // 表面からの深さ (中なので −SDF)
        if (depth < HIT_EPS) break;                   // 出口に着いた
        float s = clamp(depth, HIT_EPS, 0.02);        // 1 歩: 表面までの深さ。 2 cm まで (中の濃さの変わり方を読み飛ばさない)

        // ★ 段階 8: 気泡。 1 歩は、 マスの面までと、 気泡の表面までに抑える
        float3 bubbleC;                               // 気泡の中心
        float bubbleR;                                // 気泡の半径
        float cellStep;                               // 光線の向きに進んで、 マスを出るまでの距離
        float db = nearestBubble(p, rd, bubbleC, bubbleR, cellStep);  // 気泡の表面までの距離
        if (db < 0.0005 && dot(rd, p - bubbleC) < 0.0) {
            // 気泡の表面に着いた (近づいている向きのときだけ。 抜けた直後の裏側を、 また当たりと数えないため)。 氷 → 空気 (気泡の中) の境目なので、 浅い角度では全反射 (縁が銀色に光る)
            float3 nb = normalize(p - bubbleC);                                    // 気泡の法線 (気泡の外 = 氷の側、 光線と向かい合う)
            float3 into = refract(rd, nb, ior);                                    // スネルの法則 (氷 → 空気。 比 n / 1)
            float Fb = dot(into, into) < 1e-6 ? 1.0 : fresnelSchlick(dot(-rd, nb));  // 全反射なら全部、 そうでなければフレネルの分だけ映る
            col += thr * Fb * envMap.SampleLevel(envSampler, mul(float4(reflect(rd, nb), 0.0), world).xyz, 0).rgb;  // 気泡の映り込み
            thr *= 1.0 - Fb;                                                       // 残りは気泡を通り抜ける (小さいので曲がりは無視)
            // 気泡の向こう側へ飛ぶ: 光線と球の交点の、 遠い方 t = −b + √(b² − c) (b = oc·rd、 c = |oc|² − r²)
            float3 oc = p - bubbleC;                                               // 気泡の中心から今の点へ
            float b = dot(oc, rd);
            float c = dot(oc, oc) - bubbleR * bubbleR;
            t += -b + sqrt(max(b * b - c, 0.0)) + 0.0005;                          // 気泡の出口 + 0.5 mm
            continue;
        }
        s = min(s, min(max(db, HIT_EPS), cellStep));  // 気泡もマスの面も越えない歩幅

        // ★ 段階 9: 亀裂。 この 1 歩の間に亀裂の面を通るなら、 その点で映り込みを足す
        float dn = dot(rd, crackN);                                            // 光線の向きと、 面の法線の cos
        float tc = dot(crackC - p, crackN) / (abs(dn) > 1e-5 ? dn : 1e-5);     // 光線と平面の交点までの距離 t = (c − p)·n / (d·n)
        if (tc > 0.0 && tc < s && insideCrack(p + rd * tc - crackC)) {
            float3 nc = dn < 0.0 ? crackN : -crackN;                           // 光線と向かい合う向きの法線
            float3 through = refract(rd, nc, ior);                             // スネルの法則 (氷 → 割れ目の空気。 比 n / 1)
            float Fc = dot(through, through) < 1e-6 ? 1.0 : min(2.0 * fresnelSchlick(dot(-rd, nc)), 1.0);  // 全反射か、 2 面 (入る・出る) の映り込み
            col += thr * Fc * envMap.SampleLevel(envSampler, mul(float4(reflect(rd, nc), 0.0), world).xyz, 0).rgb;  // 亀裂の映り込み
            col += thr * (1.0 - Fc) * CRACK_WHITE * CORE_LIGHT;               // 割れ目の細かいひびで白く散らばる分
            thr *= (1.0 - Fc) * (1.0 - CRACK_WHITE);                           // 残りは割れ目を通り抜ける (薄いので曲がりは無視)
        }

        // ★ 段階 7: 白い芯 (内部散乱)。 表面から 5 cm より深い所に、 細かい気泡が集まる
        float core = coreDensity * smoothstep(0.05, 0.11, depth);  // この点の芯の濃さ σs (1/m)
        float a = 1.0 - exp(-core * s);                            // この 1 歩で気泡に当たる割合 (ランベルト・ベールの法則)
        col += thr * a * CORE_LIGHT;                               // 当たった光は白く散らばり、 目に届く (一回散乱の簡単版)
        thr *= 1.0 - a;                                            // 当たった分だけ、 奥からの光は減る
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
    if (thin != 0) {
        // ★ 段階 10: 薄い物 (コップ)。 壁で光はほとんど曲がらないので、 通った光は後ろに描いてある物 (氷・床) を使う。
        // 色 = 映り込みの光 (col)、 α = 通らなかった割合。 ブレンドが 「出した色 + 後ろの色 × (1 − α)」 なので、
        // 結果 = 映り込み + 後ろの色 × 透過率 になる (明るさのまま混ぜるので、 光の足し算として正しい)
        float alpha = 1.0 - (thr.r + thr.g + thr.b) / 3.0;  // 通らなかった割合 (透過率の平均を 1 から引く)
        return float4(col, alpha);
    }
    col += thr * envMap.SampleLevel(envSampler, mul(float4(rd, 0.0), world).xyz, 0).rgb;  // 出た向きの空と床に、 残った割合を掛けて足す
    return float4(col, 1.0);                                                  // 色 (光の強さのまま) と α (1 = 上書き)
}
