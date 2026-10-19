// =============================================================================
// GlassIce.hlsl — サンプル 06: コップの氷 (部品 1 つ = 形 1 つ)
//
// SdfObject (RenderEntity) 1 つが、 このシェーダーで形を 1 つ描く。 サンプルは氷 3 つとコップ 1 つを置く。
// 部品は形を囲む箱を描き、 箱が映る画素で光線を追う:
//   1. rayMarch       : 外から表面を探す (球追跡)。 当たらなければ描かない (discard)
//   2. 表面           : 映り込み (フレネル) を足し、 残りは屈折 (スネルの法則) して中へ
//   3. rayMarchInside : 中を出口まで進む。 吸収 (ランベルト・ベールの法則) と白い芯
//   4. 出口           : 屈折して外へ。 出られない角度なら全反射して 3 へ。 後ろの面の映り込み
//   5. 外へ出た光が同じ形にまた当たるなら 2 へ (コップの手前の壁 → 奥の壁)
//   6. 景色           : 氷は出た向きの空と床。 コップ (薄い物) は後ろに描いてある物に半透明で重ねる
// 式と仕組みの解説 (全部の部品の出典つき): samples/06_glass_ice/README.md
//
// 参考文献 (どれも式と考え方を確かめるために読んだ。 コードは写していない):
//   ・SDF の形 (角の丸い箱・円柱・殻にする onion・共通部分): Inigo Quilez "distance functions"
//       https://iquilezles.org/articles/distfunctions  (記事のコード片は MIT ライセンス)
//   ・SDF の法線 (中心差分): Inigo Quilez "normals for an SDF"
//       https://iquilezles.org/articles/normalsSDF
//   ・球追跡 (レイマーチング): Inigo Quilez "raymarching SDFs"
//       https://iquilezles.org/articles/raymarchingdf
//       原典: J. C. Hart, "Sphere Tracing", The Visual Computer 12, 1996  https://doi.org/10.1007/s003710050084
//   ・ランダム (sin のハッシュ) と値ノイズ: The Book of Shaders 第 10 章 「ランダム」・第 11 章 「ノイズ」 (日本語あり)
//       https://thebookofshaders.com/10/?lan=jp
//       https://thebookofshaders.com/11/?lan=jp
//   ・反射と屈折の向き (reflect / refract): LearnOpenGL "Cubemaps" の Reflection と Refraction
//       https://learnopengl.com/Advanced-OpenGL/Cubemaps
//       HLSL の refract (全反射の角度では (0, 0, 0) を返す):
//       https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-refract
//   ・フレネル (Schlick の近似): LearnOpenGL "PBR Theory" の Fresnel equation
//       https://learnopengl.com/PBR/Theory
//       原典: C. Schlick, Computer Graphics Forum 13(3), 1994  https://doi.org/10.1111/1467-8659.1330233
//   ・スネルの法則・全反射・ランベルト・ベールの法則 (透過率): PBR Book 第 4 版 (全文が無料で公開)
//       https://pbr-book.org/4ed/Reflection_Models/Specular_Reflection_and_Transmission
//       https://pbr-book.org/4ed/Volume_Scattering/Transmittance
//   ・内部散乱 (吸収・外への散乱・中への散乱、 放射伝達の式): PBR Book 第 4 版
//       https://pbr-book.org/4ed/Volume_Scattering/Volume_Scattering_Processes
//       このシェーダーは一回散乱・向きの偏り無し・届く光は決まった白、 と省略している (省略の仕方は独自)
//   ・冷凍庫の氷が白く濁る理由 (溶けていた空気が泡になって閉じ込められる): Wikipedia "Ice cube" の Clear and cloudy
//       https://en.wikipedia.org/wiki/Ice_cube
//   ・氷の屈折率 1.31: S. G. Warren, R. E. Brandt, J. Geophys. Res. 113, 2008  https://doi.org/10.1029/2007JD009744
//   ・後ろの面の映り込みの近似・全反射の打ち切り・コップを後ろの絵に重ねる方法・各定数の値は、 このサンプルで決めた物
//   各部品の出典と、 自分で決めた所の一覧: README.md の 14 章
// =============================================================================

#include "../../../src/engine/shaders/Environment.hlsli"  // シーンの定数 (ViewProj・太陽) と景色 (Environment / ToDisplay)

// 部品 1 つ分の定数。 C++ の SdfObject の ObjectConstants と同じ並び
cbuffer ObjectConstants : register(b1)
{
    float4x4 world;        // 部品の空間 → ワールド (転置済み。 mul(float4(p, 1), world) と書く)
    float4x4 invWorld;     // ワールド → 部品の空間 (world の逆行列。 ピクセルシェーダーの最初に 1 回だけ使う)
    float3   cameraPos;    // カメラの位置 (ワールド)
    uint     shape;        // 0 = 氷 (角の丸い立方体)、 1 = コップ
    float3   absorption;   // 1 m あたりの吸収 σ (赤・緑・青)
    float    ior;          // 屈折率 n (氷 1.31、 ガラス 1.5)
    float    coreDensity;  // 白い芯の気泡の濃さ (1/m)。 0 なら芯なし
    uint     thin;         // 1 = 薄い物 (コップ): 後ろに描いてある物に半透明で重ねる
    float2   objectPad;
};

static const float EPS = 0.001;          // 表面から少し離す距離 (自分の表面にまた当たらないように)
static const float ICE_HALF = 0.13;      // 氷の半分の大きさ
static const float ICE_ROUND = 0.035;    // 氷の角の丸み
static const float ICE_BUMP = 0.005;     // 溶けかけのでこぼこの大きさ
static const float CUP_R = 0.36;         // コップの半径 (壁の真ん中まで)
static const float CUP_H = 0.40;         // コップの高さの半分
static const float CUP_T = 0.006;        // コップの壁の厚さの半分

// ---- ノイズ ----

// ランダム: 点 → 0〜1 のでたらめな値 (sin のハッシュ)
float hash(float3 p)
{
    return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453);
}

// 値ノイズ: 格子の角 8 つにランダムな値を置き、 間をなめらかに混ぜる (−1〜1)
float noise(float3 p)
{
    float3 i = floor(p);                // マスの角 (整数の点)
    float3 f = p - i;                   // マスの中の位置 (0〜1)
    float3 u = f * f * (3.0 - 2.0 * f); // エルミート補間 3f² − 2f³ (smoothstep と同じ曲線)
    // 三線形補間: x → y → z の順に、 角 8 つの値を混ぜる
    float x00 = lerp(hash(i), hash(i + float3(1.0, 0.0, 0.0)), u.x);
    float x10 = lerp(hash(i + float3(0.0, 1.0, 0.0)), hash(i + float3(1.0, 1.0, 0.0)), u.x);
    float x01 = lerp(hash(i + float3(0.0, 0.0, 1.0)), hash(i + float3(1.0, 0.0, 1.0)), u.x);
    float x11 = lerp(hash(i + float3(0.0, 1.0, 1.0)), hash(i + float3(1.0, 1.0, 1.0)), u.x);
    return lerp(lerp(x00, x10, u.y), lerp(x01, x11, u.y), u.z) * 2.0 - 1.0;
}

// ---- SDF (形) ----

// 角の丸い箱 (半分の大きさ b、 丸み r): 箱を r だけ小さくし、 表面を r だけふくらませる
float sdRoundBox(float3 p, float3 b, float r)
{
    float3 q = abs(p) - (b - r);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;  // 箱の距離関数 − 丸み
}

// 円柱 (半径 r、 高さの半分 h): 横の距離 (中心軸から) と縦の距離を、 2 次元の箱として測る
float sdCappedCylinder(float3 p, float r, float h)
{
    float2 d = abs(float2(length(p.xz), p.y)) - float2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

// この部品の形 (部品の空間。 形の真ん中が原点)
float sdf(float3 p)
{
    if (shape == 0) {
        // 氷: 角の丸い立方体 + 溶けかけのゆるいでこぼこ (ノイズを細かくすると霜に見える)
        return sdRoundBox(p, float3(ICE_HALF, ICE_HALF, ICE_HALF), ICE_ROUND) + ICE_BUMP * noise(p * 8.0);
    }
    // コップ: 円柱を殻にし (onion: |d| − 厚さ)、 上を切って口を開ける (共通部分: max)
    float shell = abs(sdCappedCylinder(p, CUP_R, CUP_H)) - CUP_T;
    return max(shell, p.y - (CUP_H - CUP_T));
}

// 法線 = SDF の傾き (勾配)。 各軸で少し前と少し後ろの差を取る (中心差分)
float3 calcNormal(float3 p)
{
    const float2 e = float2(0.0005, 0.0);
    return normalize(float3(sdf(p + e.xyy) - sdf(p - e.xyy), sdf(p + e.yxy) - sdf(p - e.yxy),
                            sdf(p + e.yyx) - sdf(p - e.yyx)));
}

// ---- 光 ----

// フレネル (Schlick の近似): F = F0 + (1 − F0)(1 − cosθ)⁵。 浅い角度ほど強く映り込む
float fresnelSchlick(float cosTheta)
{
    float f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);  // 正面の反射率 F0 = ((n − 1) / (n + 1))² (氷 0.018、 ガラス 0.04)
    return f0 + (1.0 - f0) * pow(1.0 - saturate(cosTheta), 5.0);
}

// ---- レイマーチング ----

// 外から表面を探す (球追跡): SDF の値だけ進めば、 表面を越えない。 tMax まで進んで当たらなければ false
bool rayMarch(float3 ro, float3 rd, float tMax, out float t)
{
    t = 0.0;
    [loop]
    for (int i = 0; i < 128; ++i) {
        float d = sdf(ro + rd * t);
        if (d < 0.0002) return true;  // 表面まで 0.2 mm 未満なら当たり
        t += d * 0.9;                 // でこぼこの分、 距離が少し不正確なので 0.9 倍で控えめに
        if (t > tMax) break;
    }
    return false;
}

// 中から出口まで進む: 中では −SDF が 「いちばん近い表面までの距離」 なので、 その分進めば表面を越えない。
// 進みながら、 吸収で thr を減らし、 白い芯の気泡に当たった光を col に足す。 戻り値は進んだ距離
float rayMarchInside(float3 ro, float3 rd, inout float3 col, inout float3 thr)
{
    float t = 0.0;
    [loop]
    for (int i = 0; i < 64; ++i) {
        float depth = -sdf(ro + rd * t);                           // 表面からの深さ
        if (depth < 0.0002) break;                                  // 出口に着いた
        float s = clamp(depth, 0.0002, 0.02);                       // 1 歩 (芯の濃さを拾えるよう 2 cm まで)
        float core = coreDensity * smoothstep(0.05, 0.11, depth);   // 白い芯: 表面から 5 cm より深い所だけ
        float a = 1.0 - exp(-core * s);                             // この 1 歩で気泡に当たる割合 (ランベルト・ベールの法則)
        col += thr * a * float3(0.85, 0.9, 0.95);                   // 当たった光は白く散らばる (一回散乱の簡単版)
        thr *= (1.0 - a) * exp(-absorption * s);                    // 透過率 T = exp(−σ × 距離) (ランベルト・ベールの法則)
        t += s;
    }
    return t;
}

// ---- 頂点シェーダー: 形を囲む箱 ----

struct VSInput
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD0;
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
    v.LPos = input.pos;
    v.WPos = mul(float4(input.pos, 1.0), world).xyz;  // 部品の空間 → ワールド
    v.Pos = mul(float4(v.WPos, 1.0), ViewProj);
    v.UV = input.uv;
    return v;
}

// ---- ピクセルシェーダー ----

float4 PSMain(PSInput v, out float depth : SV_Depth) : SV_Target
{
    // 最初に 1 回だけ、 カメラを部品の空間へ (逆行列 invWorld)。 ここから先は全部、 部品の空間で計算する。
    // ワールドへ戻すのは、 景色を読む所と深度だけ (mul(float4(点, 1), world)、 向きは mul(float4(向き, 0), world))
    float3 ro = mul(float4(cameraPos, 1.0), invWorld).xyz;
    float3 rd = normalize(v.LPos - ro);  // カメラから、 この画素 (箱の裏面の点) へ

    // 1. 外から表面を探す。 箱の裏面まで進んでも当たらなければ、 この画素は描かない (後ろの物が残る)
    float t;
    if (!rayMarch(ro, rd, length(v.LPos - ro), t)) discard;
    float3 p = ro + rd * t;
    float4 clip = mul(mul(float4(p, 1.0), world), ViewProj);  // 部品の空間 → ワールド → 画面
    depth = clip.z / clip.w;  // 深度は箱の面ではなく当たった点の物 (重なった部品の前後が正しくなる)

    float3 col = float3(0.0, 0.0, 0.0);  // 目に届く光 (足していく)
    float3 thr = float3(1.0, 1.0, 1.0);  // まだ届く割合 (掛けて減らしていく)

    [loop]
    for (int entry = 0; entry < 2; ++entry) {  // 同じ形に入るのは 2 回まで
        float3 n = calcNormal(p);

        // 2. 表面: 映り込みを足し、 残りは屈折して中へ
        float F = fresnelSchlick(dot(-rd, n));
        col += thr * F * Environment(mul(float4(p, 1.0), world).xyz, mul(float4(reflect(rd, n), 0.0), world).xyz);  // 反射の向き r = d − 2(d·n)n
        thr *= 1.0 - F;
        rd = refract(rd, n, 1.0 / ior);  // スネルの法則 (空気 → 中。 屈折率の比 1 / n)
        p -= n * EPS;

        // 3〜4. 中を出口まで進む。 出られない角度なら全反射して、 また進む
        bool escaped = false;
        [loop]
        for (int k = 0; k < 4; ++k) {
            p += rd * rayMarchInside(p, rd, col, thr);
            n = calcNormal(p);
            float3 outDir = refract(rd, -n, ior);  // スネルの法則 (中 → 空気。 比 n / 1)
            if (dot(outDir, outDir) < 1e-6) {      // 解が無い = 全反射 (sinθ > 1 / n)
                rd = reflect(rd, n);
                p -= n * EPS;
                continue;
            }
            F = fresnelSchlick(dot(outDir, n));
            // 後ろの面の映り込み: 出口で中へ跳ね返る分 F を、 跳ね返った向きの景色で近似する
            col += thr * F * Environment(mul(float4(p, 1.0), world).xyz, mul(float4(reflect(rd, n), 0.0), world).xyz);
            thr *= 1.0 - F;
            rd = outDir;  // 残りは曲がって外へ
            p += n * EPS;
            escaped = true;
            break;
        }

        // 5. 外へ出た光が、 同じ形にまた当たるか (コップの手前の壁 → 奥の壁)。 当たらなければ終わり
        float tAgain;
        if (!escaped || entry == 1 || !rayMarch(p, rd, 2.0, tAgain)) break;
        p += rd * tAgain;
    }

    // 6. 景色
    if (thin != 0) {
        // 薄い物 (コップ): 壁で光はほとんど曲がらないので、 通った光は後ろに描いてある物 (氷・床) を使う。
        // 画面の色は表示用 (ToDisplay の後) なので、 「後ろの色」 と 「映り込みの平均の色」 を、 通らなかった割合 a で混ぜる:
        // 出力 = lerp(後ろの色, 映り込み, a)。 ブレンドは 出した色 + 後ろの色 × (1 − α) なので、 色に a を掛けて出す
        float a = 1.0 - (thr.r + thr.g + thr.b) / 3.0;  // 通らなかった割合 (ほとんどが映り込み)
        float3 reflection = col / max(a, 1e-4);         // 映り込みの平均の色
        return float4(ToDisplay(reflection) * a, a);
    }
    col += thr * Environment(mul(float4(p, 1.0), world).xyz, mul(float4(rd, 0.0), world).xyz);  // 氷: 出た向きの空と床
    return float4(ToDisplay(col), 1.0);
}
