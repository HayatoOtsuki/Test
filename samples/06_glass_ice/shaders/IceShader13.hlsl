// =============================================================================
// IceShader13.hlsl — 氷 (完成形)
//
// SdfObject (RenderEntity) 1 つが、 このシェーダーで氷を 1 つ描く。 サンプルは氷を 3 つ置く (コップは一旦外してある)。
// 部品は形を囲む箱の裏面を描き、 箱が映る画素で光線を追う。
//
// 【描く流れ】 (ピクセルシェーダー PSMain)
//   0. 最初に 1 回だけ、 カメラを部品の空間へ (invWorld)。 ここから先は全部、 部品の空間で計算する
//   1. rayMarch       : 外から表面を探す (球追跡)。 当たらなければ discard。 当たった点の深度を出す
//   2. 表面           : 映り込み (フレネル) を足し、 残りは屈折 (スネルの法則) して中へ
//   3. rayMarchInside : 中を出口まで進む (外へ出るまで進み、 2 分探索で詰める)。 吸収 (ランベルト・ベール)・気泡・亀裂
//      scatterInside  : 入口〜出口を一定の歩幅で進み、 体積の散乱 (層 × ノイズ・散乱の影・前方散乱) を足す
//   4. 出口           : 屈折して外へ。 出られない角度なら全反射して 3 へ。 後ろの面の映り込み
//   5. 景色           : 屈折して出た先も映り込みも、 画面のコピー (sceneCopy) から読む (画面の外なら空のグラデーション)。
//                       環境マップ (envMap) を読む行は、 コメントアウトで残してある
//   返す値は float4(色, α)。 色は光の強さのまま (表示用の変換は、 エンジンが最後に 1 回だけする)
//   エンジンの関数は使わない。 使うのは定数バッファ (ViewProj・LightDirection・LightColor と、 部品の定数) と、 画面のコピー (t1) だけ
//   (環境マップ t0 は、 コメントアウトで残してある)
//
// 【式と仕組みの一覧】 (詳しくは samples/06_glass_ice/README.md)
//   SDF                : 点から形の表面までの符号付きの距離。 外が正、 表面が 0、 中が負
//   角の丸い箱         : 箱を丸みの分小さくし、 表面を丸みの分ふくらませる
//   法線 (中心差分)    : n = ∇φ / |∇φ|。 各軸で少し前と後ろの差
//   球追跡             : φ だけ進めば表面を越えない (中では −φ。 出口は外へ出てから 2 分探索で詰める)
//   フレネル (Schlick) : F = F0 + (1 − F0)(1 − cosθ)⁵、 F0 = ((n − 1) / (n + 1))²
//   反射の向き         : r = d − 2(d·n)n
//   スネルの法則       : n1 sinθ1 = n2 sinθ2 (refract)
//   全反射             : sinθ > 1 / n で解が無い。 臨界角 = arcsin(1 / n) (氷 49.8°、 ガラス 41.8°)
//   ランベルト・ベール : 透過率 T = exp(−σ × 距離)
//   体積の散乱         : 1 歩で散乱する割合 a = 1 − exp(−σs × dt)。 col += thr × a × 届く光、 thr *= 1 − a (一回散乱)
//   散乱の影           : 届く太陽の光 × exp(−τ)、 τ = Σ σs × 歩幅 (太陽の方へ数歩)
//   位相関数 (HG)      : p(θ) = (1 − g²) / (4π (1 + g² − 2g cosθ)^(3/2))
//   画面空間の屈折     : 出口 + 向き × REFRACT_DEPTH を画面に写し (clip = mul(点, ViewProj))、 uv = clip.xy / clip.w × (0.5, −0.5) + 0.5 で読む
//   ランダム・値ノイズ : sin のハッシュ、 エルミート補間 3f² − 2f³、 三線形補間
//   光線と球の交点     : t = −b ± √(b² − c)、 b = oc·d、 c = |oc|² − r²
//   光線と平面の交点   : t = (c − p)·n / (d·n)
//
// 【このサンプルで決めた近似と値】 (文献に無い所。 説明するときは、 この区別をそのまま言う)
//   ・後ろの面の映り込み: 出口で中へ跳ね返る分を、 跳ね返った向きの景色で近似する (本当はもう一度追う)
//   ・全反射は 8 回まで。 残りは今の向きの景色で近似 (角や辺に沿った細い帯だけ)
//   ・体積の散乱: 一回散乱だけ。 届く光は 太陽 (影は 4 歩だけ調べる) + 決まった空の光 (SKY_SCATTER)。 芯の形 (4〜10 cm)・ノイズ・g の値
//   ・出口の探し方: 1 歩を 2 mm〜2 cm に収め、 外へ出てから 2 分探索 (6 回) で詰める
//   ・気泡と亀裂: 中を通り抜ける光の曲がりは無視。 亀裂の白い散らばりの割合 (CRACK_WHITE)
//   ・画面空間の屈折と映り込み: 先の物まで REFRACT_DEPTH (0.4 m) とみなす。 画面のコピーに無い物 (氷自身) は映らず、 画面の外は空のグラデーション
//   ・環境マップは、 氷のあたり (0, 0.4, 0) から見た景色 (床の模様は、 その点から見た物。 氷の場所ごとの違いは無視)
//   ・形の大きさ・でこぼこ・吸収・芯・気泡・亀裂の値は、 見た目で決めた
//
// 【参考文献】 (どれも式と考え方を確かめるために読んだ。 コードは自分で書いた。 1 行の式は文献と同じ形になる所がある (各段の 【コードの引用元】)。
//   利用条件は docs/guide/references.md、 段階ごとの解説は samples/06_glass_ice/STAGES.md)
//   ・SDF の形 (角の丸い箱): Inigo Quilez "distance functions"
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
//   ・位相関数 (Henyey-Greenstein): PBR Book 第 4 版  https://pbr-book.org/4ed/Volume_Scattering/Phase_Functions
//   ・出口の 2 分探索: Wikipedia "Bisection method"  https://en.wikipedia.org/wiki/Bisection_method
//   ・画素ごとのずらし (jitter): F. Toft ほか, "Optimisations for Real-Time Volumetric Cloudscapes", 2016  https://arxiv.org/abs/1609.05344
//   ・画面空間の屈折 (画面をテクスチャにして、 ずらした位置で読む): T. Sousa, GPU Gems 2 第 19 章, 2005
//       https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-19-generic-refraction-simulation
//   ・画面のコピー (CopyResource とリソースの状態の切り替え): Microsoft の D3D12 の説明
//       https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copyresource
//       https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12
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
//    7. 白い芯 (内部散乱の簡単版)
//    8. 気泡
//    9. 亀裂
//   10. 体積の散乱 (層 × ノイズ)
//   11. 散乱の影 (太陽への光)
//   12. 前方散乱
//   13. 周りの景色 (画面のコピー、 完成)  ← 今回
//
// 【前の段から引き継ぐ物】
//   氷の全部 (屈折・全反射・吸収・気泡・亀裂・体積の散乱)。 ここまでは景色を全部、 環境マップ (envMap) から読んでいた。
//
// 【今回足す物】
//   ・sceneCopy (t1): 背景を描いた直後の画面のコピー (エンジンが毎フレーム作る。 DrawContext::sceneCopy)
//   ・sceneBehind: 点と向きの先の景色を、 画面のコピーから読む。 画面の外なら skyColor (空のグラデーション)
//   ・環境マップを読んでいた所 (屈折した先・表面の映り込み・後ろの面・気泡・亀裂) を、 全部 sceneBehind に置き換える。
//     環境マップの行は消さずに、 コメントアウトで残してある (用意できたら戻せる)
//
// 【仕組み】
//   環境マップは 「遠くの景色」 で、 近くの物 (床・机・ほかの物) は正しい位置に映らない。 画面のコピーなら、 実際に描いた周りの物が映る。
//   画面空間の屈折: 出口の点から、 屈折した向きに REFRACT_DEPTH (0.4 m) 進んだ点を、 ViewProj で画面に写し、 その位置の色を読む。
//     先の物までの本当の距離は分からないので、 決まった距離で近似する (深度のコピーも読めば、 もっと正しくできる)。
//     uv = clip.xy / clip.w × (0.5, −0.5) + 0.5 (テクスチャは y が下向きなので、 y だけ符号を反対にする)。
//   画面のコピーには、 コピーより後に描いた物 (氷自身・ほかの氷) は入っていない。 自分のエンジンでは、 不透明な物を全部描いた後にコピーして、
//     その後に氷 (透ける物) を描く。
//   映り込み (反射) も同じ関数で読むが、 反射の向きはたいてい画面の外を向くので、 多くは空のグラデーションになる。
//     床の方へ向く反射だけは、 画面のコピーの床が映る (位置は近似)。 正しい反射には、 環境マップか、 画面空間の反射 (SSR) が要る。
//
// 【確かめること】
//   ・氷を通した床の模様が、 氷の後ろの本当の床とつながって見える (段階 12 は、 環境マップの床なので少しずれていた)
//   ・画面の端に寄せると、 画面の外を読む所だけ空のグラデーションに切り替わる
//
// 【参考】
//   T. Sousa, "Generic Refraction Simulation", GPU Gems 2 第 19 章, 2005
//     (場面をテクスチャに描き、 ずらした位置で読んで屈折に見せる)
//     https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-19-generic-refraction-simulation
//   Microsoft "CopyResource" (画面のコピー)  https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copyresource
//   Microsoft "Using Resource Barriers" (コピーの前後で、 リソースの状態を切り替える)
//     https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12
//
// 【コードの引用元】
//   sceneBehind: GPU Gems 2 第 19 章の考え方 (画面をテクスチャにして、 ずらした位置で読む) から自分で書いた。
//     あの章のコード (© NVIDIA、 転載不可) は写していない。 「奥へ 0.4 m 進めた点を画面に写す」 ずらし方は、 このサンプルで決めた
//   skyColor: 自分で書いた (色は見た目で決めた)
//   画面のコピー (Renderer.cpp): Microsoft の CopyResource とリソースバリアの説明から自分で書いた
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
// TextureCube<float4> envMap : register(t0);  (環境マップ。 段階 13 では使わない。 用意できたらコメントを戻し、 envMap を読む行も戻す)
SamplerState envSampler : register(s0);  // 線形補間 (C++ の静的サンプラー)
// 周りの景色: 背景を描いた直後の画面のコピー (光の強さのまま)。 屈折した先の景色を、 ここから読む。 段階 13 から使う
Texture2D<float4> sceneCopy : register(t1);
static const float REFRACT_DEPTH = 0.4;  // 屈折した先の景色が、 出口からどれだけ奥にあるとみなすか (m)
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
static const int SCATTER_STEPS = 32;                        // 体積の散乱で、 入口から出口まで進む歩数 (一定の歩幅)
static const float SUN_SCATTER = 0.3;                       // 散乱する光のうち、 太陽から来る分の強さ (太陽の色 × これ)
static const float3 SKY_SCATTER = float3(0.30, 0.35, 0.42); // 散乱する光のうち、 空から来る分 (どの向きからも来る弱い光)
static const float SCATTER_G = 0.5;                         // 前方散乱の強さ g (−1〜1。 0 で偏り無し、 正で光の進む向きへ散りやすい)

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

// ---- 体積の散乱 (段階 10〜12) ----

// 中の濃さ σs (1/m): SDF の層 (表面からの深さ) で芯の形を決め、 3D ノイズでむらを付ける
float scatterDensity(float3 p)
{
    float depth = -sdf(p);                         // 表面からの深さ (SDF の層。 中なので −SDF)
    float core = smoothstep(0.04, 0.10, depth);    // 外側 4 cm は透明、 10 cm より深いと最大
    float n = noise(p * 12.0) * 0.5 + 0.5;         // 3D ノイズ (0〜1)。 12 で 8 cm ごとのむら
    return coreDensity * core * (0.4 + 0.6 * n);   // むらのある芯
}

// 位相関数 (Henyey-Greenstein): 散乱で光が曲がる向きの偏り。 g > 0 で、 光の進む向きのまま前へ散りやすい。
// p(θ) = (1 − g²) / (4π (1 + g² − 2g cosθ)^(3/2))。 ここでは 4π を掛けて、 偏りが無い (g = 0) とき 1 になるようにしてある
float phaseHG(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
}

// 入口 p0 から出口 p1 まで、 一定の歩幅で SCATTER_STEPS 歩進み、 目の方へ散った光を返す (体積レンダリング)。
// thr (まだ届く割合) も、 散乱で外れた分だけ減らす (吸収は rayMarchInside が掛けている)。 jitter: 画素ごとのずらし (0〜1)
float3 scatterInside(float3 p0, float3 p1, float jitter, inout float3 thr)
{
    float len = length(p1 - p0);                                                  // 区間の長さ
    float3 dir = (p1 - p0) / max(len, 1e-5);                                      // 進む向き
    float dt = len / float(SCATTER_STEPS);                                        // 1 歩の長さ (一定)
    float3 sunLocal = normalize(mul(float4(LightDirection, 0.0), invWorld).xyz);  // ★ 段階 11: 太陽への向き (部品の空間。 向きなので w = 0)
    float phase = phaseHG(dot(dir, sunLocal), SCATTER_G);                         // ★ 段階 12: 太陽 → この点 → 目 の曲がり方 (目が太陽の方を向くほど明るい)
    float3 col = float3(0.0, 0.0, 0.0);                                           // 目の方へ散った光 (足していく)
    [loop]
    for (int i = 0; i < SCATTER_STEPS; ++i) {
        float3 p = p0 + dir * ((float(i) + jitter) * dt);                         // 歩の位置 (jitter で画素ごとにずらし、 層の縞を消す)
        float sigma = scatterDensity(p);                                          // この点の濃さ σs
        if (sigma <= 0.0) continue;
        float a = 1.0 - exp(-sigma * dt);                                         // この 1 歩で散乱する割合 (ランベルト・ベールの法則)
        float3 sun = LightColor * SUN_SCATTER;                                    // この点に届く太陽の光
        // ★ 段階 11: 太陽の方へ 4 歩 (2 cm ずつ) 進み、 芯で減った分を掛ける。 τ = Σ σs × 歩幅 (光学的な厚さ)、 届く割合 = exp(−τ)
        float tau = 0.0;
        [loop]
        for (int k = 1; k <= 4; ++k) tau += scatterDensity(p + sunLocal * (0.02 * float(k))) * 0.02;
        sun *= exp(-tau);
        sun *= phase;                                                             // ★ 段階 12: 前方散乱
        col += thr * a * (sun + SKY_SCATTER);                                     // 散った光のうち、 目の方へ来る分 (一回散乱)
        thr *= 1.0 - a;                                                           // 散乱で外れた分だけ、 奥からの光は減る
    }
    return col;
}

// ---- 周りの景色 (段階 13) ----

// 画面の外を読むときの代わりの空: 地平線から天頂へのグラデーション (背景の空と同じ色)。 環境マップが用意できたら、 そちらを読む
float3 skyColor(float3 dw)
{
    return lerp(float3(0.62, 0.72, 0.85), float3(0.16, 0.34, 0.70), saturate(dw.y * 1.6));  // 地平線の色 → 天頂の色
}

// 点 pw (ワールド) から向き dw (ワールド) の先に見える周りの景色を、 画面のコピーから読む。
// 先の物までの距離は分からないので、 REFRACT_DEPTH だけ奥にあるとみなし、 その点を画面に写して読む (画面空間の屈折)。
// 屈折した先だけでなく、 映り込み (反射) もこれで読む (反射の向きはたいてい画面の外なので、 その時は空の色になる)。
// 画面の外になったら (コピーに無いので) 空のグラデーションで代わりにする
float3 sceneBehind(float3 pw, float3 dw)
{
    float4 clip = mul(float4(pw + dw * REFRACT_DEPTH, 1.0), ViewProj);  // 奥の点 → クリップ空間
    float2 uv = clip.xy / clip.w * float2(0.5, -0.5) + 0.5;             // 透視の割り算 → −1〜1 → 0〜1 (テクスチャは y が下向き)
    if (clip.w <= 0.0 || uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        // return envMap.SampleLevel(envSampler, dw, 0).rgb;  (環境マップ。 用意できたら、 こちらに戻す)
        return skyColor(dw);                                            // 画面の外 (コピーに無い): 空のグラデーション
    }
    return sceneCopy.SampleLevel(envSampler, uv, 0).rgb;               // 画面のコピーの、 その位置の色
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
    float3 crackC;                                    // ★ 段階 9: 亀裂の円盤の中心
    float3 crackN = crackPlane(crackC);               // ★ 段階 9: 亀裂の面の法線
    float t = 0.0;                                    // 進んだ距離
    float tIn = 0.0;                                  // 中だと分かっている、 いちばん遠い距離 (2 分探索に使う)
    [loop]
    for (int i = 0; i < 128; ++i) {                   // 歩数の上限
        float3 p = ro + rd * t;                       // 今いる点
        float depth = -sdf(p);                        // 表面からの深さ (中なので −SDF)
        if (depth < 0.0) break;                       // 外へ出た (出口を越えた)
        tIn = t;                                      // ここはまだ中
        float s = clamp(depth, 0.002, 0.02);          // 1 歩: 表面までの深さ。 2 mm〜2 cm (小さすぎると歩数を使い切り、 大きいと中の濃さを読み飛ばす)

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
            // col += thr * Fb * envMap.SampleLevel(envSampler, mul(float4(reflect(rd, nb), 0.0), world).xyz, 0).rgb;  // 気泡の映り込み  (環境マップ。 用意できたら、 こちらに戻す)
            col += thr * Fb * sceneBehind(mul(float4(p, 1.0), world).xyz, mul(float4(reflect(rd, nb), 0.0), world).xyz);  // 気泡の映り込み  // ★ 段階 13: 画面のコピーから
            thr *= 1.0 - Fb;                                                       // 残りは気泡を通り抜ける (小さいので曲がりは無視)
            // 気泡の向こう側へ飛ぶ: 光線と球の交点の、 遠い方 t = −b + √(b² − c) (b = oc·rd、 c = |oc|² − r²)
            float3 oc = p - bubbleC;                                               // 気泡の中心から今の点へ
            float b = dot(oc, rd);
            float c = dot(oc, oc) - bubbleR * bubbleR;
            t += -b + sqrt(max(b * b - c, 0.0)) + 0.0005;                          // 気泡の出口 + 0.5 mm
            continue;                                                              // (気泡は氷の中にしか置かないので、 抜けた先も氷の中)
        }
        s = min(s, min(max(db, 0.0005), cellStep));   // 気泡もマスの面も越えない歩幅

        // ★ 段階 9: 亀裂。 この 1 歩の間に亀裂の面を通るなら、 その点で映り込みを足す
        float dn = dot(rd, crackN);                                            // 光線の向きと、 面の法線の cos
        float tc = dot(crackC - p, crackN) / (abs(dn) > 1e-5 ? dn : 1e-5);     // 光線と平面の交点までの距離 t = (c − p)·n / (d·n)
        if (tc > 0.0 && tc < s && insideCrack(p + rd * tc - crackC)) {
            float3 nc = dn < 0.0 ? crackN : -crackN;                           // 光線と向かい合う向きの法線
            float3 through = refract(rd, nc, ior);                             // スネルの法則 (氷 → 割れ目の空気。 比 n / 1)
            float Fc = dot(through, through) < 1e-6 ? 1.0 : min(2.0 * fresnelSchlick(dot(-rd, nc)), 1.0);  // 全反射か、 2 面 (入る・出る) の映り込み
            // col += thr * Fc * envMap.SampleLevel(envSampler, mul(float4(reflect(rd, nc), 0.0), world).xyz, 0).rgb;  // 亀裂の映り込み  (環境マップ。 用意できたら、 こちらに戻す)
            col += thr * Fc * sceneBehind(mul(float4(p + rd * tc, 1.0), world).xyz, mul(float4(reflect(rd, nc), 0.0), world).xyz);  // 亀裂の映り込み  // ★ 段階 13: 画面のコピーから
            col += thr * (1.0 - Fc) * CRACK_WHITE * CORE_LIGHT;               // 割れ目の細かいひびで白く散らばる分
            thr *= (1.0 - Fc) * (1.0 - CRACK_WHITE);                           // 残りは割れ目を通り抜ける (薄いので曲がりは無視)
        }
        thr *= exp(-absorption * s);                  // ★ 段階 6: 吸収。 透過率 T = exp(−σa × 距離) (ランベルト・ベールの法則)
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
    // col += thr * F * envMap.SampleLevel(envSampler, mul(float4(reflect(rd, n), 0.0), world).xyz, 0).rgb;  // 反射の向き r = d − 2(d·n)n の景色  (環境マップ。 用意できたら、 こちらに戻す)
    col += thr * F * sceneBehind(mul(float4(p, 1.0), world).xyz, mul(float4(reflect(rd, n), 0.0), world).xyz);  // 反射の向き r = d − 2(d·n)n の景色  // ★ 段階 13: 画面のコピーから
    thr *= 1.0 - F;                                                           // 残りの割合が中へ入る
    rd = refract(rd, n, 1.0 / ior);                                           // ★ 段階 4: 屈折。 スネルの法則 (空気 → 中。 屈折率の比 1 / n)
    p -= n * EPS;                                                             // 少し中へ入れてから進む
    float jitter = hash(float3(v.Pos.xy, 0.0));                               // ★ 段階 10: 画素ごとのずらし (0〜1)。 体積の散乱の層の縞を消す

    // 3〜4. 中を出口まで進む。 出られない角度なら全反射して、 また進む
    [loop]
    for (int k = 0; k < 8; ++k) {                                             // 全反射は 8 回まで (角の近くでは何度も跳ね返る)
        float3 pIn = p;                                                       // ★ 段階 10: この区間の入口
        p += rd * rayMarchInside(p, rd, col, thr);                            // 3. 出口 (後ろの面) まで進む
        col += scatterInside(pIn, p, jitter, thr);                            // ★ 段階 10: 入口〜出口の体積の散乱 (中では光はまっすぐ進む)
        n = calcNormal(p);                                                    // 出口の法線
        float3 outDir = refract(rd, -n, ior);                                 // スネルの法則 (中 → 空気。 比 n / 1。 法線は光線と向かい合う −n)
        if (dot(outDir, outDir) < 1e-6) {                                     // ★ 段階 5: 解が無い (refract が (0, 0, 0)) = 全反射 (sinθ > 1 / n)
            rd = reflect(rd, n);                                              // 中で跳ね返る
            p -= n * EPS;                                                     // 少し中へ戻す
            continue;                                                         // また出口を探す
        }
        F = fresnelSchlick(dot(outDir, n));                                   // 出口で映り込む割合
        // ★ 段階 5: 後ろの面の映り込み。 中へ跳ね返る分 F を、 跳ね返った向きの景色で近似して足す
        // col += thr * F * envMap.SampleLevel(envSampler, mul(float4(reflect(rd, n), 0.0), world).xyz, 0).rgb;  (環境マップ。 用意できたら、 こちらに戻す)
        col += thr * F * sceneBehind(mul(float4(p, 1.0), world).xyz, mul(float4(reflect(rd, n), 0.0), world).xyz);  // ★ 段階 13: 画面のコピーから
        thr *= 1.0 - F;                                                       // 残りは外へ
        rd = outDir;                                                          // 曲がって外へ
        break;
    }

    // 5. 景色: 出た向きの景色に、 残った割合を掛けて足す
    // col += thr * envMap.SampleLevel(envSampler, mul(float4(rd, 0.0), world).xyz, 0).rgb;  (環境マップ。 用意できたら、 こちらに戻す)
    col += thr * sceneBehind(mul(float4(p, 1.0), world).xyz, mul(float4(rd, 0.0), world).xyz);  // ★ 段階 13: 屈折した先は、 画面のコピー (周りの景色) から
    return float4(col, 1.0);                                                  // 色 (光の強さのまま) と α (1 = 上書き)
}
