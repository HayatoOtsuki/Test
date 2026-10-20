# 氷のシェーダーを段階ごとに作る (解説書)

[`IceShader0.hlsl`](shaders/IceShader0.hlsl) 〜 [`IceShader13.hlsl`](shaders/IceShader13.hlsl) を、 **作る順に** 説明する。
段階ごとに、 次の 9 つを書いた。

| 見出し | 書いてあること |
| --- | --- |
| ねらい | この段で、 画面の何が変わるか |
| 考え方 | なぜそうするのか。 現実の氷で何が起きているか |
| 仕組み (式) | 使う式と、 その意味 |
| 実装の手順 | 前の段のファイルに、 何をどの順で足すか |
| コード | 足す所の抜き出し (完成形と同じ行。 コメントは短くした) |
| 確かめ方 | 動かして何を見れば、 合っていると分かるか |
| つまずきやすい所 | 作ったときに実際に起きた失敗と、 その直し方 |
| 参考文献 | 下の [参考文献の一覧](#参考文献の一覧) の番号 |
| コードの引用元 | そのコードを誰が書いたか。 どこから来た考え方か |

[README](README.md) は 「部品ごとの辞書」 (フレネルとは何か、 など)。 この文書は 「作る順の手引き」。 同じ式は README にもっと詳しい説明がある。

## 出典の書き方

- **出典あり**: 文献に書いてある式や考え方。 番号 [n] を付けた。
- **このサンプルで決めた**: 文献に無い近似や値。 見た目を見ながら決めた。 説明するときも、 この区別をそのまま言う。
- **コードの引用元**: コードは全部このリポジトリで書いた。 文献のコード (Book of Shaders・LearnOpenGL・GPU Gems など、 写してはいけない物) は写していない。
  ただし 1 行の式 (角の丸い箱、 光線と球の交点など) は、 誰が書いても文献と同じ形になる。 その所は 「文献の式と同じ形」 と書いた。
  利用条件は [docs/guide/references.md](../../docs/guide/references.md) にまとめた。

## 目次

- [全体の考え方](#全体の考え方)
- [段階 0: 箱を描く](#段階-0-箱を描く)
- [段階 1: 形を探す (球追跡)](#段階-1-形を探す-球追跡)
- [段階 2: 法線と光 (不透明)](#段階-2-法線と光-不透明)
- [段階 3: 透明にする (映り込みと透過)](#段階-3-透明にする-映り込みと透過)
- [段階 4: 屈折 (歪み)](#段階-4-屈折-歪み)
- [段階 5: 全反射と後ろの面](#段階-5-全反射と後ろの面)
- [段階 6: 吸収と溶けかけの形](#段階-6-吸収と溶けかけの形)
- [段階 7: 白い芯 (内部散乱の簡単版)](#段階-7-白い芯-内部散乱の簡単版)
- [段階 8: 気泡](#段階-8-気泡)
- [段階 9: 亀裂](#段階-9-亀裂)
- [段階 10: 体積の散乱 (層 × ノイズ)](#段階-10-体積の散乱-層--ノイズ)
- [段階 11: 散乱の影](#段階-11-散乱の影)
- [段階 12: 前方散乱](#段階-12-前方散乱)
- [段階 13: 周りの景色 (画面のコピー、 完成)](#段階-13-周りの景色-画面のコピー-完成)
- [自分のエンジンへ移すとき](#自分のエンジンへ移すとき)
- [参考文献の一覧](#参考文献の一覧)

---

## 全体の考え方

### 1. 形はメッシュではなく式 (SDF) で持つ

透明な氷では、 **表面の向こう側** (後ろの面・中の気泡・亀裂・濁り) が見える。 三角形のメッシュは表面しか持たないので、 「中のこの点は表面から何 cm か」 が分からない。
SDF (符号付き距離関数) は、 どの点でも 「いちばん近い表面までの距離」 を返す (外が正、 中が負) [2]。 これがあると:

- 外から表面を探せる (球追跡、 段階 1) [1][4]
- 中から出口を探せる (段階 4)
- 中の点の **深さ** (−SDF) が分かるので、 「芯ほど濁る」 「芯ほど気泡が多い」 が 1 行で書ける (段階 7・8・10)

### 2. 箱を描いて、 箱が映る画素で光線を追う

ピクセルシェーダーは、 何かを描いた画素でしか動かない。 そこで、 氷を囲む箱のメッシュを描き、 箱が映った画素で光線を追う (C++ の `SdfObject`)。
箱は **裏面** を描く (cull FRONT)。 表面を描くと、 カメラが箱の中に入ったときに何も描かれなくなるため (このサンプルで決めた)。

### 3. 計算は全部、 部品の空間で

氷は回したり傾けたりする。 形の式 (SDF) は 「真ん中が原点で、 回っていない」 空間で書きたい。
そこで、 ピクセルシェーダーの最初に **1 回だけ** カメラを `invWorld` で部品の空間へ移し、 その後は全部その空間で計算する。
ワールドに戻すのは、 「景色を読む向き」 と 「深度」 のときだけ (`mul(…, world)` をその場に書く)。

### 4. 光は 「足す色 col」 と 「残りの割合 thr」 で運ぶ

光線は表面で 2 つに分かれる (映り込む分と、 中へ入る分)。 両方を追うと、 分かれるたびに光線が倍になって重い。
そこで、 **片方 (映り込み) はその場で景色を読んで col に足し、 もう片方 (中へ入る分) だけを追う**。 追う光の強さの割合が thr。

```
col += thr * F * 映り込んだ向きの景色   // 映り込む分は、 ここで足して終わり
thr *= 1 - F                              // 残りが中へ入る
...                                        // 中で吸収・散乱されるたびに thr が減る
col += thr * 最後に出た向きの景色         // 最後まで残った分
```

これは光線の追跡でよく使われる、 「足す色」 と 「残りの割合」 を分ける書き方 (このリポジトリで書いた)。 段階 3 から最後まで、 この形は変わらない。

### 5. 足していくだけで、 作り直さない

段階 N のファイルは、 段階 N − 1 のファイルに **行を足した物**。 足した行には `★ 段階 N` の印がある。 前の段のファイルと並べて見比べると、 足した所だけが分かる。
例外は 2 つだけ: 段階 7 の白い芯は段階 10 で置き換わり、 段階 3〜12 の環境マップは段階 13 でコメントアウトになる。

---

## 段階 0: 箱を描く

**ねらい**: 氷を囲む箱が、 色の付いた箱として見える。

**考え方**: 光線を追う前に、 「箱が正しい場所に、 正しい大きさで出ているか」 と 「頂点シェーダーからピクセルシェーダーへ、 部品の空間の位置が届いているか」 を確かめる。
後の段階で何かがおかしいとき、 ここが合っていれば、 C++ 側と行列は疑わなくてよい。

**仕組み (式)**:
- 部品の空間 → ワールド → 画面 を、 頂点シェーダーで 1 回だけ変換する。 このリポジトリは行ベクトルの流儀 (`mul(点, 行列)`) なので、 C++ は行列を転置して送る。
- 点は `float4(p, 1)` (w = 1。 移動が効く)、 向きは `float4(d, 0)` (w = 0。 移動が効かない)。 段階 1 以降でずっと使う区別。

**実装の手順**:
1. 定数 `ObjectConstants` (b1) に `world`・`invWorld`・`cameraPos` などを並べる (C++ の構造体と同じ並び、 32 bit × 40 個)。
2. 頂点シェーダー: `v.LPos` (部品の空間)、 `v.WPos` (ワールド)、 `v.Pos` (画面) を作る。
3. ピクセルシェーダー: `v.LPos` を 0〜1 に直して色にする。

**コード**:

```hlsl
PSInput VSMain(VSInput input)
{
    PSInput v;
    v.LPos = input.pos;                               // 部品の空間
    v.WPos = mul(float4(input.pos, 1.0), world).xyz;  // → ワールド (1 回だけ)
    v.Pos = mul(float4(v.WPos, 1.0), ViewProj);       // → 画面
    v.UV = input.uv;
    return v;
}

float4 PSMain(PSInput v) : SV_Target
{
    float3 c = v.LPos / 0.3 + 0.5;    // −0.15〜+0.15 を 0〜1 に
    return float4(saturate(c), 1.0);
}
```

**確かめ方**: 色の付いた箱が 3 つ見える。 +x の面が赤っぽい。 カメラを箱の中に入れても消えない (裏面を描いているため)。

**つまずきやすい所**: 箱が出ない・ゆがむときは、 C++ で行列を転置して送っているかを見る (`transpose(world)`)。

**参考文献**: なし (このリポジトリの行列の流儀に合わせただけ)。

**コードの引用元**: 自分で書いた。

---

## 段階 1: 形を探す (球追跡)

**ねらい**: 箱の中に、 角の丸い白い立方体が見える。 重なった氷の前後が正しい。

**考え方**: カメラから画素へ向かう光線を、 少しずつ進めて表面を探す。 SDF は 「いちばん近い表面までの距離 d」 なので、 **d だけ進んでも、 どの表面も越えない**。
だから、 表面から遠い所では大きく、 近い所では小さく進める。 これが球追跡 (sphere tracing) [1][4]。

**仕組み (式)**:
- 角の丸い箱 [2]: 箱を丸みの分 r だけ小さくし、 表面を r だけふくらませる。
  `q = |p| − (b − r)`、 `φ = |max(q, 0)| + min(max(qx, qy, qz), 0) − r`
- 球追跡 [1][4]: `t ← t + φ(ro + rd t)` を、 `φ < 0.2 mm` (当たり) か、 箱の裏面を越える (外れ) まで。
- 深度: 当たった点を画面に写して `clip.z / clip.w` を `SV_Depth` に書く [23]。 箱の面の深度のままだと、 重なった氷の前後がおかしくなる。
- 当たらない画素は `discard` (描かない) [24]。

**実装の手順**:
1. `sdRoundBox` と `sdf` を書く。
2. `rayMarch(ro, rd, tMax, out t)` を書く。 `tMax` は 「カメラから箱の裏面まで」 (`length(v.LPos − ro)`)。
3. PSMain の最初で、 カメラを部品の空間へ (`invWorld`、 w = 1)。 光線の向きは 「カメラ → `v.LPos`」。
4. 当たらなければ `discard`。 当たったら深度を出して、 白を返す。

**コード**:

```hlsl
float sdRoundBox(float3 p, float3 b, float r)
{
    float3 q = abs(p) - (b - r);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

bool rayMarch(float3 ro, float3 rd, float tMax, out float t)
{
    t = 0.0;
    [loop]
    for (int i = 0; i < 128; ++i) {
        float d = sdf(ro + rd * t);
        if (d < HIT_EPS) return true;  // 0.2 mm
        t += d * 0.9;                  // 控えめに進む
        if (t > tMax) break;
    }
    return false;
}

// PSMain の最初
float3 ro = mul(float4(cameraPos, 1.0), invWorld).xyz;  // カメラ → 部品の空間 (1 回だけ)
float3 rd = normalize(v.LPos - ro);
float t;
if (!rayMarch(ro, rd, length(v.LPos - ro), t)) discard;
float3 p = ro + rd * t;
float4 clip = mul(mul(float4(p, 1.0), world), ViewProj);
depth = clip.z / clip.w;
```

**確かめ方**: 白い角の丸い立方体のシルエット。 氷どうしが重なる所で、 手前の氷が奥の氷を隠す。

**つまずきやすい所**:
- 0.9 倍にしているのは、 段階 6 でノイズを足すと SDF が 「本当の距離より少し大きい」 所ができ、 1 倍だと表面を越えてしまうため (このサンプルで決めた)。
- `[loop]` は 「ループを展開しないで、 本当にループとしてコンパイルして」 というコンパイラへの指示。 128 回を展開するとコンパイルが遅く、 シェーダーも大きくなる。

**参考文献**: [1] [2] [4] [23] [24]

**コードの引用元**: `sdRoundBox` は、 [2] の角の丸い箱と同じ式 (1 行の式なので、 誰が書いても同じ形になる。 記事のコード片は MIT)。
`rayMarch` は [1][4] の考え方から自分で書いた。 0.9 倍・0.2 mm・128 歩は、 このサンプルで決めた値。

---

## 段階 2: 法線と光 (不透明)

**ねらい**: 立方体に陰影が付き、 太陽の側が明るくなる。 J / L で太陽を回すと、 明るい面が回る。

**考え方**: 光の計算には面の向き (法線) が要る。 メッシュなら頂点に法線を持たせるが、 SDF は式なので、 **式の傾き** から法線を作る。
SDF の値は表面から離れるほど大きくなるので、 「値がいちばん速く増える向き」 (勾配) が、 表面から外へ向かう向き = 法線 [3]。

**仕組み (式)**:
- 中心差分 [3]: `n = normalize(φ(p + h x) − φ(p − h x), φ(p + h y) − φ(p − h y), φ(p + h z) − φ(p − h z))`、 h = 0.5 mm。
- ランバート: 明るさ = `max(n · L, 0)` (L は太陽への向き)。 面が太陽に正対すると 1、 横向きで 0。
- 太陽の向きはワールドなので、 法線をワールドへ直してから内積を取る (向きなので w = 0)。

**実装の手順**:
1. `calcNormal(p)` を書く (SDF を 6 回呼ぶ)。
2. PSMain で、 法線をワールドへ直し、 ランバートの明るさ × 太陽の色 + 空からの弱い光 を返す。

**コード**:

```hlsl
float3 calcNormal(float3 p)
{
    const float2 e = float2(0.0005, 0.0);
    return normalize(float3(sdf(p + e.xyy) - sdf(p - e.xyy),
                            sdf(p + e.yxy) - sdf(p - e.yxy),
                            sdf(p + e.yyx) - sdf(p - e.yyx)));
}

float3 nw = normalize(mul(float4(n, 0.0), world).xyz);      // 法線 → ワールド
float diffuse = saturate(dot(nw, LightDirection));          // ランバート
float3 lit = albedo * (LightColor * diffuse + float3(0.22, 0.30, 0.42));
```

**確かめ方**: 立方体の、 太陽の側の面が明るく、 反対側が暗い (空の光の分だけ真っ黒にはならない)。 角の丸みに沿って明るさがなめらかに変わる。

**つまずきやすい所**: 法線を部品の空間のまま `LightDirection` (ワールド) と内積すると、 氷を回したときに明るい面がずれる。 空間をそろえる。

**参考文献**: [3]

**コードの引用元**: `calcNormal` は [3] の中心差分と同じ形 (`e.xyy` のような書き方も記事の流儀)。 ずらす幅 0.5 mm と色の値は、 このサンプルで決めた。
この段の不透明な光 (ランバート) は、 段階 3 で透明にすると使わなくなる (形と法線を確かめるための段)。

---

## 段階 3: 透明にする (映り込みと透過)

**ねらい**: 氷がうっすらとしか見えなくなる。 正面は透明で、 縁だけが空を映して明るい。 これが 「屈折の無い透明」 (まだ氷に見えない)。

**考え方**: 透明な物の見た目は、 ほぼ **「表面で映り込む光」 と 「通り抜けた光」** の 2 つでできている。
どれだけ映り込むかは、 見る角度で大きく変わる。 正面から見るとほとんど映らず (氷で 1.8%)、 浅い角度ほど鏡のように映る。 これがフレネル反射 [10][11][12]。
ここで 「全体の考え方」 の 4 の col と thr の形を入れる。

**仕組み (式)**:
- Schlick の近似 [11]: `F = F0 + (1 − F0)(1 − cosθ)⁵`。 θ は光線と法線のなす角。
- 正面の反射率 [12]: `F0 = ((n − 1) / (n + 1))²`。 氷 n = 1.31 [18] で F0 = 0.018、 ガラス n = 1.5 で 0.04。
- 反射の向き: `r = d − 2(d·n)n` (HLSL の `reflect`)。
- 景色: この段から段階 12 までは、 環境マップ (キューブマップ) を向きで読む [9]。

**実装の手順**:
1. `fresnelSchlick(cosθ)` を書く。 `F0` は屈折率 `ior` (定数) から作る。
2. PSMain で `col`・`thr` を作る。
3. `col += thr * F * 反射の向きの景色`、 `thr *= 1 − F`、 `col += thr * まっすぐの向きの景色`。

**コード**:

```hlsl
float fresnelSchlick(float cosTheta)
{
    float f0 = pow((ior - 1.0) / (ior + 1.0), 2.0);
    return f0 + (1.0 - f0) * pow(1.0 - saturate(cosTheta), 5.0);
}

float3 col = float3(0.0, 0.0, 0.0);
float3 thr = float3(1.0, 1.0, 1.0);
float3 n = calcNormal(p);
float F = fresnelSchlick(dot(-rd, n));
col += thr * F * envMap.SampleLevel(envSampler, mul(float4(reflect(rd, n), 0.0), world).xyz, 0).rgb;
thr *= 1.0 - F;
col += thr * envMap.SampleLevel(envSampler, mul(float4(rd, 0.0), world).xyz, 0).rgb;  // まっすぐ透かす
```

**確かめ方**: ほとんど見えない。 縁 (浅い角度の所) だけ空が映る。 「透明なだけでは氷に見えない」 ことを確かめる段。

**つまずきやすい所**: `cosθ` は `dot(-rd, n)` (光線は面に向かうので、 符号を反対にする)。 符号を間違えると、 正面が鏡になる。

**参考文献**: [9] [10] [11] [12] [18]

**コードの引用元**: `fresnelSchlick` は [11] の式をそのまま書いた。 [10] にも同じ式の関数があるが、 あちらのコード (CC BY-NC) は写していない。 col と thr の形は自分で書いた。

---

## 段階 4: 屈折 (歪み)

**ねらい**: 氷を通した床の模様が曲がる。 **ここで急に氷らしくなる**。 斜めから見ても、 中の面が二重に重なって見えない。

**考え方**: 光は、 屈折率の違う物の境目で曲がる (スネルの法則 [12])。 氷に入るときに 1 回、 出るときにもう 1 回曲がる。
「歪み」 は特別な処理ではなく、 **曲がった向きで景色を読むこと** そのもの。 面の向き・中を通る長さ・出口の面の向きで、 ずれ方が勝手に変わる。
出口を知るには、 中から 「後ろの面」 を探す必要がある。 ここで SDF が効く: 中では −SDF が 「いちばん近い表面までの距離」 になる。

**仕組み (式)**:
- スネルの法則 [12]: `n1 sinθ1 = n2 sinθ2`。 HLSL の `refract(入る向き, 光線と向かい合う法線, n1 / n2)` が曲がった向きを返す [22]。
  - 入口 (空気 → 氷): `refract(rd, n, 1 / ior)`
  - 出口 (氷 → 空気): `refract(rd, −n, ior)` (出口の法線 n は外向きなので、 光線と向かい合うのは −n)
- 出口の探し方 (このサンプルで決めた形。 2 分探索は [19]):
  1. 1 歩を `clamp(−SDF, 2 mm, 2 cm)` にして、 **SDF が正 (外) になるまで** 進む。 最後に中だった距離を `tIn`、 外へ出た距離を `t` とする。
  2. 出口は `tIn` と `t` の間にある。 真ん中 `m` を調べ、 外なら `t = m`、 中なら `tIn = m`。 これを 6 回 (区間が 1/64 になる。 2 cm の歩なら 0.3 mm)。

**実装の手順**:
1. 入口で屈折した向きに変え、 法線の反対へ `EPS` (1 mm) だけ中へ入れる (同じ表面にまた当たらないように)。
2. `rayMarchInside(ro, rd, col, thr)` を書く。 戻り値は出口までの距離。
3. 出口で法線を取り直し、 `refract(rd, −n, ior)` で外へ。 出口でも一部は映り込んで中へ戻るので、 `thr *= 1 − F`。
4. 最後に、 曲がった向きの景色を `thr` 倍で足す。
5. この段では全反射をまだ扱わない (`refract` が 0 を返したら、 まっすぐ出してしまう)。

**コード**:

```hlsl
float rayMarchInside(float3 ro, float3 rd, inout float3 col, inout float3 thr)
{
    float t = 0.0;
    float tIn = 0.0;                              // 中だと分かっている、 いちばん遠い距離
    [loop]
    for (int i = 0; i < 128; ++i) {
        float depth = -sdf(ro + rd * t);          // 表面からの深さ
        if (depth < 0.0) break;                   // 外へ出た
        tIn = t;
        t += clamp(depth, 0.002, 0.02);           // 2 mm〜2 cm
    }
    [loop]
    for (int j = 0; j < 6; ++j) {                 // 2 分探索
        float m = 0.5 * (tIn + t);
        if (sdf(ro + rd * m) > 0.0) t = m;
        else tIn = m;
    }
    return t;
}

// PSMain
rd = refract(rd, n, 1.0 / ior);                    // 入口: 空気 → 氷
p -= n * EPS;
p += rd * rayMarchInside(p, rd, col, thr);         // 出口まで
n = calcNormal(p);
float3 outDir = refract(rd, -n, ior);              // 出口: 氷 → 空気
if (dot(outDir, outDir) < 1e-6) outDir = rd;       // 段階 4 では全反射をまだ扱わない
thr *= 1.0 - fresnelSchlick(dot(outDir, n));
rd = outDir;
col += thr * envMap.SampleLevel(envSampler, mul(float4(rd, 0.0), world).xyz, 0).rgb;
```

**確かめ方**: 床の模様が曲がる。 角の近くで景色が大きく引き伸ばされる。 斜めから見ても、 中の面が重ならない。

**つまずきやすい所** (実際に起きた失敗):
- 最初は 「−SDF だけ進み、 表面まで 0.2 mm を切ったら出口」 としていた。 すると、 **斜めの光線が面とほぼ平行に、 表面のすぐ近くを進む** とき、
  −SDF が小さいので歩幅も小さくなり、 128 歩を使い切ったり、 本当は出口ではないのに出口と判断したりした。 そこで出た向きで景色を読むので、
  **斜めから見ると、 奥の面と手前の面が重なって見えた**。
  → 歩幅に下限 2 mm を付け、 「外へ出たと分かってから、 2 分探索で詰める」 形にして直した。 上限 2 cm は、 段階 7 以降で中の濃さを読み飛ばさないため。
- 中へ入れる `EPS` を忘れると、 入口の点で SDF ≈ 0 なので、 すぐ 「外へ出た」 と判断する。

**参考文献**: [12] [19] [22]

**コードの引用元**: `refract`・`reflect` は HLSL の組み込み関数。 `rayMarchInside` (外へ出るまで進み、 2 分探索で詰める) は自分で書いた。 2 分探索は教科書どおりの形 [19]。 2 mm〜2 cm・6 回は、 このサンプルで決めた値。

---

## 段階 5: 全反射と後ろの面

**ねらい**: 面ごとに明るい所と暗い所ができ、 縁が銀色に光る。 斜めに見た後ろの面が光る。

**考え方**: 氷 → 空気 のように、 **屈折率の大きい物から小さい物へ** 出るとき、 角度が浅いと光は外へ出られず、 全部はね返る (全反射 [12][16])。
氷やガラスがきらきら見えるのは、 中から出ようとする光の多くが出られないため。 段階 4 では、 出られない光もまっすぐ出していたので、 ここで正す。

**仕組み (式)**:
- 出口では `sinθ2 = 1.31 sinθ1`。 これが 1 を超えると、 スネルの法則を満たす向きが無い。
  臨界角 `θc = arcsin(1 / n)` = 49.8° (氷)、 41.8° (ガラス)。
- HLSL の `refract` は、 このとき `(0, 0, 0)` を返す [22]。 長さが 0 なら全反射。
- 全反射したら、 中で `reflect` して、 また出口を探す (8 回まで)。
- 後ろの面の映り込み (このサンプルで決めた近似): 出口で、 屈折して出られた場合でも F の分は中へはね返る。
  本当はその光をもう一度追うが、 光線が 2 本に分かれて重いので、 「はね返った向きの景色 × F」 を足すだけにする。

**実装の手順**:
1. PSMain の 「中を進む → 出口」 を、 `for (k < 8)` のループで囲む。
2. 出口で `refract` の長さが 0 なら、 `reflect` して少し中へ戻し、 `continue`。
3. 出られたら、 後ろの面の映り込み `col += thr * F * 反射の向きの景色` を足して `break`。

**コード** (完成形と同じ形。 段階 10 の行を除く):

```hlsl
[loop]
for (int k = 0; k < 8; ++k) {
    p += rd * rayMarchInside(p, rd, col, thr);
    n = calcNormal(p);
    float3 outDir = refract(rd, -n, ior);
    if (dot(outDir, outDir) < 1e-6) {     // 全反射
        rd = reflect(rd, n);
        p -= n * EPS;
        continue;
    }
    F = fresnelSchlick(dot(outDir, n));
    col += thr * F * 反射の向きの景色;     // 後ろの面の映り込み (近似)
    thr *= 1.0 - F;
    rd = outDir;
    break;
}
```

**確かめ方**: 面ごとのまだら、 縁の銀色。 J / L で太陽を回しても、 まだらの場所はほとんど動かない (空の映り込みなので)。

**つまずきやすい所**:
- 最初は上限 4 回だった。 角や辺の近くでは何度も跳ね返るので、 4 回では足りない画素が、 辺に沿った細い帯として出た (デバッグで赤く塗って確かめた)。 8 回にして、 帯はごく細くなった。
- 斜めから見たときの 「白い面」 は、 ほとんどがこの全反射の空の映り込み。 おかしな描画ではない (確かめ済み)。

**参考文献**: [12] [16] [22]

**コードの引用元**: 全反射の判定 (長さ 0) は [22] の説明から。 8 回のループと後ろの面の近似は、 自分で書いた (このサンプルで決めた)。

---

## 段階 6: 吸収と溶けかけの形

**ねらい**: 屈折した床の模様が、 ゆるく波打つ。 色はほとんど変わらない (小さな氷はほぼ無色)。

**考え方**:
- 本物の氷は、 冷凍庫の中や溶けかけで、 表面がわずかにでこぼこしている。 面の向きが場所ごとに少しずつ違うので、 屈折した景色が波打つ。 完全に平らだとガラスに見える。
- 氷は赤い光をわずかに吸う (厚い氷河が青い理由)。 ただし、 コップに入る大きさでは通る長さが短いので、 ほぼ無色。 吸収を強くすると 「氷塊」 に見える。

**仕組み (式)**:
- ランダム (sin のハッシュ) [6]: `hash(p) = frac(sin(p · k) × 43758.5453)`。 sin を大きく引き伸ばして小数の部分だけを取ると、 でたらめに見える。
- 値ノイズ [7]: 空間を 1 の大きさのマスに分け、 マスの 8 つの角に hash の値を置き、 間を混ぜる。 混ぜる重みは、 そのまま (線形) ではなく、
  エルミート曲線 `u = 3f² − 2f³` (smoothstep と同じ) [7]。 マスの境目で傾きが 0 になるので、 継ぎ目が見えない。
- でこぼこ: `SDF + 5 mm × noise(p × 8)`。 ノイズを足すと 「本当の距離」 ではなくなるので、 段階 1 の 0.9 倍が効く。
- ランベルト・ベールの法則 [13]: 光が長さ L 進むと、 残る割合は `T = exp(−σa L)`。 1 歩 s ごとに `exp(−σa s)` を掛ければ、 全部で `exp(−σa L)`。
  σa は赤・緑・青で別の値 (C++ の `setAbsorption`、 1 m あたり)。

**実装の手順**:
1. `hash` と `noise` を書く。
2. `sdf` に `ICE_BUMP * noise(p * BUMP_FREQ)` を足す。
3. `rayMarchInside` の 1 歩ごとに `thr *= exp(-absorption * s)`。

**コード**:

```hlsl
float hash(float3 p)
{
    return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453);
}

float noise(float3 p)
{
    float3 i = floor(p);
    float3 f = p - i;
    float3 u = f * f * (3.0 - 2.0 * f);   // エルミート曲線
    float x00 = lerp(hash(i), hash(i + float3(1, 0, 0)), u.x);
    float x10 = lerp(hash(i + float3(0, 1, 0)), hash(i + float3(1, 1, 0)), u.x);
    float x01 = lerp(hash(i + float3(0, 0, 1)), hash(i + float3(1, 0, 1)), u.x);
    float x11 = lerp(hash(i + float3(0, 1, 1)), hash(i + float3(1, 1, 1)), u.x);
    return lerp(lerp(x00, x10, u.y), lerp(x01, x11, u.y), u.z) * 2.0 - 1.0;  // −1〜1
}

d += ICE_BUMP * noise(p * BUMP_FREQ);   // sdf の中: 5 mm のでこぼこ
thr *= exp(-absorption * s);            // rayMarchInside の 1 歩ごと: 吸収
```

**確かめ方**: 模様がゆるく波打つ。 C++ の `setAbsorption` を 10 倍にすると青っぽくなる (試したら戻す)。

**つまずきやすい所**: `ICE_BUMP` を大きくしすぎると、 球追跡の 0.9 倍でも表面を越えて穴があく。 `BUMP_FREQ` を大きくすると (細かくすると) 霜に見える。

**参考文献**: [6] [7] [13]

**コードの引用元**: `hash` は [6] の sin のハッシュの考え方 (43758.5453 は [6] の章のコードにも出てくる、 広く使われている値。 内積の係数 127.1, 311.7, 74.7 もよく使われている値)。
`noise` は [7] の値ノイズ (章の例は 1D・2D) を、 3D (8 つの角の三線形補間) に自分で広げた。 吸収は [13] の式をそのまま使った。 5 mm・8 と吸収の値は、 このサンプルで決めた。

---

## 段階 7: 白い芯 (内部散乱の簡単版)

**ねらい**: 氷の真ん中が白く濁り、 縁の近くは透明なまま。

**考え方**: 冷凍庫の氷は外側から凍る。 水に溶けていた空気が、 凍っていない内側へ押し出され、 最後に凍る芯に細かい泡として閉じ込められる。 だから **芯だけが白く濁る** [17]。
光が泡の多い所を進むと、 泡に当たって向きが変わる (散乱)。 散った光の一部が目に届くので、 白く光って見える。 その分、 奥からの光は減る。
SDF なら 「表面からの深さ」 (−SDF) がそのまま分かるので、 「深い所ほど濃い」 を 1 行で書ける。

**仕組み (式)**:
- 1 歩 s で泡に当たる割合 [13][14]: `a = 1 − exp(−σs s)` (ランベルト・ベールの法則と同じ形)。
- 増える分: `col += thr × a × 届く光`。 減る分: `thr *= 1 − a`。
- 濃さ: `σs = coreDensity × smoothstep(5 cm, 11 cm, 深さ)`。
- 本当は放射伝達の式 [14] で、 「どの向きへ散るか」 「その点にどれだけ光が届くか」 を計算する。
  ここでは **一回だけ散る・向きの偏りは無し・届く光は決まった白 (`CORE_LIGHT`)** と省略した (このサンプルで決めた近似)。

**実装の手順**: `rayMarchInside` の 1 歩ごとに、 上の 3 行を足す (吸収の前)。

**コード** (段階 7〜9 だけ。 段階 10 で置き換わる):

```hlsl
float core = coreDensity * smoothstep(0.05, 0.11, depth);  // 芯の濃さ
float a = 1.0 - exp(-core * s);                            // この 1 歩で泡に当たる割合
col += thr * a * CORE_LIGHT;                               // 白く散って目に届く
thr *= 1.0 - a;                                            // 奥からの光は減る
```

**確かめ方**: 芯が白く濁る。 C++ の `setCoreDensity(0)` で芯が消える (お店の透明な氷)。

**つまずきやすい所**: 全体に濃さを入れると、 霧の玉のように白っぽくなり、 透明感が消える。 外側を透明に残すのが大事。
また、 この形は歩幅が 2 mm〜2 cm で変わるので、 歩の位置がそろう所に縞が出やすい。 それも段階 10 で直す。

**参考文献**: [13] [14] [17]

**コードの引用元**: 式は [13] から。 「届く光を決まった白にする」 省略と、 5〜11 cm の値は、 このサンプルで決めた。

---

## 段階 8: 気泡

**ねらい**: 氷の中に、 縁の光る小さな粒が散らばる。 芯の近くほど多い。

**考え方**:
- 気泡は小さな **空気の球**。 光は 氷 → 空気 と通るので、 出口と同じく、 浅い角度では全反射する [16]。 だから気泡は、 縁が銀色に光り、 真ん中が透ける粒に見える。
- 気泡を 1 つずつ配列に持つと、 数が増えるほど重い。 そこで、 **空間をマスに分け、 マスの番号から hash で 「気泡があるか・位置・大きさ」 を決める** [8]。
  同じマスからはいつも同じ気泡が出るので、 置き場所を覚えなくてよい。

**仕組み (式)**:
- マス: 3.5 cm。 番号 `cell = floor(p / 3.5 cm)`、 でたらめな値 3 つ `hash3(cell + 部品の位置 × 13)` (部品の位置を混ぜて、 氷ごとに並びを変える)。
- 気泡がある確率: 中心が深いほど多い `5% + 25% × smoothstep(3 cm, 10 cm, 深さ)`。 半径 1.5〜4 mm。
- 気泡はマスの面から 30% 離して置く。 だから **自分のマスの気泡だけ** を調べればよい。 そのかわり、 1 歩は 「光線がこのマスを出るまで」 に抑える。
  マスを出るまでの距離は、 各軸で、 光線が進む向きにある面までの `t = (面 − p) / rd` のいちばん小さい物 (光線と箱の交差、 スラブ法と同じ式)。
- 気泡に当たったら (このサンプルで決めた近似):
  - 映り込み: 全反射なら 1、 そうでなければフレネル `F`。 `col += thr × F × 反射の向きの景色`、 `thr *= 1 − F`。
  - 抜け方: 光線と球の交点 [5] `t = −b + √(b² − c)` (`b = oc·d`、 `c = |oc|² − r²`) で、 気泡の出口まで一気に飛ぶ。
    本当は気泡の中で光が広がる (凹レンズ) が、 小さいので曲がりは無視する。

**実装の手順**:
1. `hash3` と `nearestBubble(p, rd, out center, out radius, out cellStep)` を書く。 気泡が無ければ 1000 を返す。
2. `rayMarchInside` で、 気泡まで 0.5 mm 未満、 かつ **気泡に近づいている** (`dot(rd, p − c) < 0`) なら、 映り込みを足して出口へ飛ぶ。
3. 1 歩を `min(歩幅, 気泡までの距離, マスを出るまで)` にする。

**コード**:

```hlsl
float3 cell = floor(p / BUBBLE_CELL);
float3 r = hash3(cell + seed * 13.0);
float3 boundary = (cell + step(0.0, rd)) * BUBBLE_CELL;        // 進む向きにある面
float3 tFace = (boundary - p) / safeRd;                         // t = (面 − p) / rd
cellStep = min(tFace.x, min(tFace.y, tFace.z)) + 0.0005;        // マスを出る距離

// rayMarchInside の中
if (db < 0.0005 && dot(rd, p - bubbleC) < 0.0) {                // 気泡に近づいて当たった
    float3 nb = normalize(p - bubbleC);
    float3 into = refract(rd, nb, ior);                         // 氷 → 空気
    float Fb = dot(into, into) < 1e-6 ? 1.0 : fresnelSchlick(dot(-rd, nb));
    col += thr * Fb * 反射の向きの景色;
    thr *= 1.0 - Fb;
    float3 oc = p - bubbleC;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - bubbleR * bubbleR;
    t += -b + sqrt(max(b * b - c, 0.0)) + 0.0005;               // 気泡の出口へ
    continue;
}
s = min(s, min(max(db, 0.0005), cellStep));
```

**確かめ方**: 縁の光る粒。 3 つの氷で並びが違う。 芯の白さが、 気泡を入れる前と同じくらい残っている。

**つまずきやすい所** (実際に起きた失敗: 気泡を入れたら白い芯が消えた。 原因は 2 つ):
- マスの歩幅を 「いちばん近いマスの面までの距離」 にしていた。 光線が面と平行に近いと、 その距離がずっと小さいまま進まず、 歩数を使い切った。
  → 「光線の向きに進んで、 マスを出るまでの距離」 に直した。
- 気泡を抜けた直後、 まだ気泡のすぐ近くなので、 同じ気泡にまた当たったと判断していた。
  → 「気泡に近づいているときだけ当たり」 (`dot(rd, p − c) < 0`) にした。

**参考文献**: [5] [8] [16]

**コードの引用元**: 「マスの番号から hash で点を決める」 は [8] の考え方。 3D の気泡に使う形は自分で書いた。 マスを出るまでの距離は自分で書いた (スラブ法と同じ式)。
気泡を抜ける式は、 [5] の光線と球の交点の式と同じ形。 3.5 cm・1.5〜4 mm・5〜30% は、 このサンプルで決めた。

---

## 段階 9: 亀裂

**ねらい**: 氷の中に、 角度によって銀色に光る面が見える。 正面から見るとほとんど透明で、 斜めほど光る。

**考え方**: 亀裂は **薄い空気の層**。 光は 氷 → 空気 → 氷 と通るので、 気泡と同じく、 浅い角度では全反射して鏡のように光る [16]。
正面から見ると、 2 つの面 (入る・出る) の映り込みだけで、 ほとんど透ける。 本物の亀裂は細かいひびで光を白く散らすので、 少しだけ白も足す。

**仕組み (式)**:
- 形: 部品の位置から hash3 で向きと中心を決めた平面の上の、 半径 8 cm の円盤。 ふちの半径を `0.7〜1.2 倍` にノイズでゆらして、 ぎざぎざにする。
- 光線と平面の交点 [5]: `t = (c − p)·n / (d·n)`。 その t が今の 1 歩 (0〜s) の中にあり、 交点が円盤の中なら通った。
- 映り込み: 全反射なら 1、 そうでなければ `2F` (面が 2 つ。 このサンプルで決めた近似)。
- 白い散らばり: 通る光の 8% (`CRACK_WHITE`) を白として足す (このサンプルで決めた値)。

**実装の手順**:
1. `crackPlane(out center)` と `insideCrack(q)` を書く。
2. `rayMarchInside` の 1 歩ごとに、 平面との交点を調べ、 通ったら映り込みと白を足す。

**コード**:

```hlsl
float dn = dot(rd, crackN);
float tc = dot(crackC - p, crackN) / (abs(dn) > 1e-5 ? dn : 1e-5);   // t = (c − p)·n / (d·n)
if (tc > 0.0 && tc < s && insideCrack(p + rd * tc - crackC)) {
    float3 nc = dn < 0.0 ? crackN : -crackN;                          // 光線と向かい合う法線
    float3 through = refract(rd, nc, ior);
    float Fc = dot(through, through) < 1e-6 ? 1.0 : min(2.0 * fresnelSchlick(dot(-rd, nc)), 1.0);
    col += thr * Fc * 反射の向きの景色;
    col += thr * (1.0 - Fc) * CRACK_WHITE * CORE_LIGHT;
    thr *= (1.0 - Fc) * (1.0 - CRACK_WHITE);
}
```

**確かめ方**: 斜めから見ると銀色の面。 ふちがぎざぎざ。 3 つの氷で向きが違う。

**つまずきやすい所**: 交点を調べる区間を 「今の 1 歩」 に限らないと、 同じ亀裂を何度も数えてしまう。

**参考文献**: [5] [16]

**コードの引用元**: 光線と平面の交点は [5] の式と同じ形。 円盤・ぎざぎざ・2F・8% は、 自分で書いた (このサンプルで決めた)。

---

## 段階 10: 体積の散乱 (層 × ノイズ)

**ねらい**: 芯が、 むらのある雲のように白く濁る。 縁の 4 cm は透明。 層の縞が出ない。

**考え方**: 段階 7 の白い芯には 3 つの弱点があった。
1. 濃さが深さだけで決まるので、 つるっとした霧の玉に見える。
2. 歩幅が場所で変わる (2 mm〜2 cm) ので、 歩の位置がそろう所に縞が出る。
3. 届く光が決まった白なので、 太陽を回しても芯が変わらない。

そこで、 散乱は **別の関数 `scatterInside`** に分け、 「入口から出口まで」 を一定の歩幅で歩く (体積レンダリング [14])。
氷の中では光はまっすぐ進むので、 入口と出口 (段階 4 の `rayMarchInside` が見つける) の間を等分すればよい。 全反射で跳ね返ったら、 区間ごとに呼ぶ。
濃さは SDF の層 (深さ) で芯の形を決め、 3D ノイズを掛けてむらを付ける。 これは 「SDF の層を 32 層サンプリングする」 考え方を、 「入口〜出口を 32 歩」 として実装した物。

**仕組み (式)**:
- 1 歩 `dt = 区間の長さ / 32`。 各歩で `a = 1 − exp(−σs dt)` [13]、 `col += thr × a × 届く光`、 `thr *= 1 − a` (一回散乱 [14])。
- 濃さ: `σs = coreDensity × smoothstep(4 cm, 10 cm, 深さ) × (0.4 + 0.6 × noise(p × 12))`。
- 届く光 (この段): 太陽 `LightColor × 0.3` + 空 `SKY_SCATTER`。 向きの偏りは無し (等方 [15])。
- **画素ごとのずらし (jitter)** [20]: 32 歩だと、 全部の画素で同じ深さの所を読むので層の縞が出る。 歩の位置を、 画素ごとに 0〜1 歩でたらめにずらすと、
  縞が細かいざらつきに変わって見えなくなる。 `jitter = hash(画面の位置)`。
  [20] は時間方向のぼかし (TAA) と組み合わせて、 ざらつきも消している。 このサンプルには TAA が無いので、 細かいざらつきは少し残る (近寄ると見える)。

**実装の手順**:
1. 段階 7 の白い芯の 4 行を消す (定数 `CORE_LIGHT` は亀裂で使うので残す)。
2. `scatterDensity(p)` と `scatterInside(p0, p1, jitter, thr)` を書く。
3. PSMain で、 入口の屈折の後に `jitter` を作る。
4. 全反射のループの中で、 `rayMarchInside` の前の位置を `pIn` に取っておき、 後で `scatterInside(pIn, p, jitter, thr)` を足す。

**コード** (この段の形):

```hlsl
float scatterDensity(float3 p)
{
    float depth = -sdf(p);                        // SDF の層 (表面からの深さ)
    float core = smoothstep(0.04, 0.10, depth);   // 外側 4 cm は透明
    float n = noise(p * 12.0) * 0.5 + 0.5;        // むら
    return coreDensity * core * (0.4 + 0.6 * n);
}

float3 scatterInside(float3 p0, float3 p1, float jitter, inout float3 thr)
{
    float len = length(p1 - p0);
    float3 dir = (p1 - p0) / max(len, 1e-5);
    float dt = len / float(SCATTER_STEPS);        // 一定の歩幅
    float3 col = float3(0.0, 0.0, 0.0);
    [loop]
    for (int i = 0; i < SCATTER_STEPS; ++i) {
        float3 p = p0 + dir * ((float(i) + jitter) * dt);   // 画素ごとにずらす
        float sigma = scatterDensity(p);
        if (sigma <= 0.0) continue;
        float a = 1.0 - exp(-sigma * dt);
        float3 sun = LightColor * SUN_SCATTER;
        col += thr * a * (sun + SKY_SCATTER);
        thr *= 1.0 - a;
    }
    return col;
}

// PSMain
float jitter = hash(float3(v.Pos.xy, 0.0));
...
float3 pIn = p;
p += rd * rayMarchInside(p, rd, col, thr);
col += scatterInside(pIn, p, jitter, thr);
```

**確かめ方**: 芯が雲のようにむらになる。 縁の 4 cm は透明。 `jitter` を 0 にすると縞が出る (試したら戻す)。 C++ の `setCoreDensity` を上げると牛乳のように、 下げると透明に近づく。

**つまずきやすい所**:
- 散乱を `rayMarchInside` の中に入れたままにすると、 歩幅が場所で変わるので縞が残る。 区間を等分するために、 関数を分けた。
- `thr` を `inout` で渡し忘れると、 芯の奥の景色が濁らないまま透ける。

**参考文献**: [13] [14] [15] [20]

**コードの引用元**: `scatterInside` は [14] の放射伝達の式を 「一回散乱・一定の歩幅」 に省いて自分で書いた。 jitter は [20] の考え方。 hash(画面の位置) で作る形は自分で書いた。
`scatterDensity` の形と値 (4〜10 cm・12・0.4 + 0.6)、 32 歩、 0.3、 `SKY_SCATTER` は、 このサンプルで決めた。

---

## 段階 11: 散乱の影

**ねらい**: 芯の、 太陽の側が明るく、 反対側が少し暗い。 J / L で太陽を回すと、 明るい側が回る。

**考え方**: 段階 10 では、 芯のどの点にも同じだけ太陽の光が届いていた。 本当は、 芯の奥の点に届く光は、 **芯を通ってくる間に減っている**。
その点から太陽の方へ数歩進んで、 間にある濃さを足せば、 どれだけ減ったかが分かる。 これで芯に立体感が出る。

**仕組み (式)**:
- 光学的な厚さ [13]: `τ = Σ σs(p + L × 2 cm × k) × 2 cm` (k = 1〜4)。
- 届く割合 [13]: `exp(−τ)` (ランベルト・ベールの法則)。 太陽の光にこれを掛ける。
- 太陽の向きはワールドなので、 `invWorld` で部品の空間へ直す (向きなので w = 0)。 区間の最初に 1 回だけ。
- 重さ: 4 歩 × 32 歩 = 1 画素で 128 回の濃さの計算 (区間ごと)。 重ければ歩数を減らす。

**実装の手順**: `scatterInside` の最初に `sunLocal` を作り、 各歩で `tau` を足してから `sun *= exp(-tau)`。

**コード**:

```hlsl
float3 sunLocal = normalize(mul(float4(LightDirection, 0.0), invWorld).xyz);  // 太陽への向き (部品の空間)
...
float tau = 0.0;
[loop]
for (int k = 1; k <= 4; ++k) tau += scatterDensity(p + sunLocal * (0.02 * float(k))) * 0.02;
sun *= exp(-tau);
```

**確かめ方**: 太陽の側の芯が明るい。 太陽を回すと明るい側が回る。

**つまずきやすい所**: 4 歩 × 2 cm = 8 cm しか見ないので、 氷の反対の端までの影は入らない (このサンプルで決めた省略)。 芯の厚さが 10 cm ほどなので、 見た目には十分。

**参考文献**: [13] [14]

**コードの引用元**: 自分で書いた。 `exp(−τ)` は [13] の透過率の式。 4 歩・2 cm は、 このサンプルで決めた。

---

## 段階 12: 前方散乱

**ねらい**: 太陽の方を向いて (逆光で) 見ると、 芯がぼんやり光る。 太陽を背にして見ると、 段階 11 より少し暗い。

**考え方**: 小さな泡や粒に当たった光は、 どの向きにも同じだけ散るのではなく、 **進んでいた向きのまま前へ散りやすい**。
だから、 太陽を背にした氷を、 太陽の方を向いて見ると、 芯が明るく光る (雲の縁が逆光で光るのと同じ)。 散る向きの偏りを表す関数が 位相関数 [15]。

**仕組み (式)**:
- Henyey-Greenstein の位相関数 [15]: `p(θ) = (1 − g²) / (4π (1 + g² − 2g cosθ)^(3/2))`。 g (−1〜1) が偏りの強さ。 0 で偏り無し、 正で前へ。
- θ は 「太陽 → 散る点 → 目」 で光が曲がる角度。 光は太陽から `−sunLocal` の向きに来て、 目へは `−dir` の向きに出るので、
  `cosθ = (−sunLocal)·(−dir) = dir · sunLocal`。 目が太陽の方を向くほど 1 に近づく。
- このサンプルでは `4π` を掛けた形 (`phaseHG` に `1 / (4π)` を書かない)。 偏りが無い (g = 0) とき 1 になり、 段階 11 と明るさを比べやすい。
  g = 0.5 では、 真正面 (cosθ = 1) で 6 倍、 真後ろ (cosθ = −1) で 0.22 倍。

**実装の手順**:
1. `phaseHG(cosθ, g)` を書く。
2. `scatterInside` の最初に `phase = phaseHG(dot(dir, sunLocal), SCATTER_G)` を 1 回だけ (区間の中では dir も sunLocal も変わらない)。
3. 太陽の光に `phase` を掛ける。 空の光 (どの向きからも来る) には掛けない。

**コード**:

```hlsl
float phaseHG(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);   // × 4π した形 (g = 0 で 1)
}

float phase = phaseHG(dot(dir, sunLocal), SCATTER_G);
...
sun *= phase;
```

**確かめ方**: 逆光で芯が光る。 `SCATTER_G` を 0 にすると、 段階 11 と同じになる。

**つまずきやすい所**: `cosθ` の向きを反対にすると、 太陽を背にしたときに光る (逆になる)。 「目が太陽の方を向くと明るい」 で確かめる。

**参考文献**: [15]

**コードの引用元**: `phaseHG` は [15] の式から、 `1 / (4π)` を省いた形を自分で書いた。 g = 0.5 は、 このサンプルで決めた。

---

## 段階 13: 周りの景色 (画面のコピー、 完成)

**ねらい**: 氷を通した床の模様が、 氷の後ろの本当の床とつながって見える。

**考え方**: 環境マップは 「ある 1 点から見た、 無限に遠い景色」。 近くの物 (床・机・ほかの物) は、 氷の場所によって見える位置が変わるので、 正しい位置に映らない。
そこで、 **実際に描いた画面をコピーして、 そこから読む** (画面空間の屈折 [21])。 光線を曲げた先の点を画面に写し、 その位置の色を読めば、 周りの物がずれて見える。
環境マップの行は消さずに、 コメントアウトで残してある (自分のエンジンで環境マップを用意できたら戻せる)。

**仕組み (式)**:
- 画面のコピー (C++): エンジンが背景を描いた直後に、 画面 (HDR の `SceneColorBuffer`) を別のテクスチャへ `CopyResource` する [25]。
  コピーの前後で、 リソースの状態を切り替える [26]:
  画面 RENDER_TARGET → COPY_SOURCE、 コピー先 PIXEL_SHADER_RESOURCE → COPY_DEST、 コピー、 戻す。 コピー先は `DrawContext::sceneCopy` で部品へ渡し、 t1 で読む。
- 画面空間の屈折 (このサンプルで決めた形。 考え方は [21]):
  1. 出口の点から、 屈折した向きに `REFRACT_DEPTH` (0.4 m) 進んだ点を、 先の物があるとみなす (本当の距離は分からないので、 決まった距離で近似)。
  2. その点を `ViewProj` で画面に写す: `clip = mul(点, ViewProj)`。
  3. 透視の割り算をして 0〜1 にする: `uv = clip.xy / clip.w × (0.5, −0.5) + 0.5` (テクスチャは y が下向きなので、 y だけ符号を反対にする)。
  4. 画面の外 (`uv` が 0〜1 の外、 または `clip.w ≤ 0` = カメラの後ろ) なら、 コピーに無いので空のグラデーション `skyColor`。
- 映り込み (反射) も同じ関数で読む。 反射の向きは多くが画面の外を向くので、 たいていは空のグラデーションになる。 床の方へ向く反射だけ、 画面のコピーの床が映る (位置は近似)。

**実装の手順**:
1. C++: エンジンの `Renderer::beginFrame` で、 背景を描いた直後にコピーする (上の 4 つの状態の切り替え)。 部品の SRV ヒープの t1 にコピー先の SRV を作る。
2. シェーダー: `Texture2D<float4> sceneCopy : register(t1)` と `REFRACT_DEPTH` を足す。
3. `skyColor(dw)` と `sceneBehind(pw, dw)` を書く。 引数はワールドの点と向き (`mul(…, world)` をその場で書く)。
4. 景色を読んでいた 5 か所 (表面の映り込み・後ろの面・屈折した先・気泡・亀裂) の `envMap.SampleLevel(…)` を、 `sceneBehind(…)` に置き換え、 元の行はコメントにする。

**コード**:

```hlsl
Texture2D<float4> sceneCopy : register(t1);
// TextureCube<float4> envMap : register(t0);   // 環境マップ (用意できたら戻す)

float3 skyColor(float3 dw)
{
    return lerp(float3(0.62, 0.72, 0.85), float3(0.16, 0.34, 0.70), saturate(dw.y * 1.6));
}

float3 sceneBehind(float3 pw, float3 dw)
{
    float4 clip = mul(float4(pw + dw * REFRACT_DEPTH, 1.0), ViewProj);
    float2 uv = clip.xy / clip.w * float2(0.5, -0.5) + 0.5;
    if (clip.w <= 0.0 || uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        return skyColor(dw);
    }
    return sceneCopy.SampleLevel(envSampler, uv, 0).rgb;
}

// PSMain の最後
col += thr * sceneBehind(mul(float4(p, 1.0), world).xyz, mul(float4(rd, 0.0), world).xyz);
// col += thr * envMap.SampleLevel(envSampler, mul(float4(rd, 0.0), world).xyz, 0).rgb;  (環境マップ。 用意できたら戻す)
return float4(col, 1.0);
```

C++ (エンジンの `Renderer.cpp`、 背景を描いた直後):

```cpp
transition(mSceneColor.getResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
transition(mSceneCopy.getResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
mCommandList->CopyResource(mSceneCopy.getResource(), mSceneColor.getResource());
transition(mSceneCopy.getResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
transition(mSceneColor.getResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
context.sceneCopy = mSceneCopy.getResource();
```

**確かめ方**: 氷を通した床が、 後ろの本当の床とつながる。 氷を画面の端に寄せると、 画面の外を読む所だけ空のグラデーションに切り替わる。

**つまずきやすい所**:
- コピーより後に描いた物 (氷自身・ほかの氷) は、 コピーに入っていない。 氷を通してほかの氷は見えない。 自分のエンジンでは、 **不透明な物を全部描いた後にコピーし、 その後に氷 (透ける物) を描く**。
- コピーは画面と同じ形式・同じ大きさにする (`CopyResource` の条件 [25])。 ウィンドウの大きさを変えたら作り直す。
- 0.4 m は場面の大きさで変わる。 小さすぎると屈折のずれが小さく、 大きすぎると画面の外を読む所が増える。 正しくするには、 深度のコピーも読んで、 先の物までの距離を求める。
- 反射を正しくするには、 環境マップか、 画面空間の反射 (SSR) が要る。 このサンプルは近似のまま。

**参考文献**: [21] [25] [26]

**コードの引用元**: `sceneBehind` は [21] の考え方 (場面をテクスチャに描き、 ずらした位置で読む) から自分で書いた。 [21] のコード (© NVIDIA、 転載不可) は写していない。
「奥へ 0.4 m 進めた点を画面に写す」 ずらし方は、 このサンプルで決めた ([21] は法線のマップでずらす)。 `skyColor` は自分で書いた (色は見た目で決めた)。 画面のコピーは [25][26] の説明から自分で書いた。

---

## 自分のエンジンへ移すとき

| このサンプル | 自分のエンジンでは |
| --- | --- |
| `SdfObject` (部品 1 つ = 氷 1 つ) が箱を描く | 同じ。 コンポーネントを付けた物ごとに、 箱 (裏面) を描く |
| 頂点シェーダーで `world` を 1 回、 ピクセルシェーダーの最初に `invWorld` を 1 回 | 同じ。 自分のエンジンの逆行列 (InverseTrans) の定数を使う |
| `v.Pos`・`v.LPos`・`v.WPos`・`v.UV` | 同じ名前にしてある |
| 景色 = 画面のコピー (t1) | 不透明な物を全部描いた後にコピーし、 その後に氷を描く |
| 環境マップ (t0) はコメントアウト | スカイボックスのキューブマップを t0 に渡し、 コメントを外す |
| 色は光の強さのまま返す (`float4(色, α)`) | 表示用の変換 (トーンマップ・ガンマ) は、 エンジンが最後に 1 回だけ |
| 屈折率 1.31・吸収・芯の濃さ (C++ のセッター) | 同じ値から始めて、 見た目で調整する |

足していく順も、 この文書の段階の順がよい。 **段階 4 (屈折) までで、 氷らしさの大部分ができる**。 段階 10 (体積の散乱) で、 ガラスと氷の違い (芯の濁り) が出る。

---

## 参考文献の一覧

URL はすべて 2026-10-21 に開けることを確かめた。 利用条件の詳しい所は [docs/guide/references.md](../../docs/guide/references.md)。
「読む・式を使う」 はどれも可。 下の 「コード」 の列は、 その文献のコードを写してよいか (このリポジトリでは、 どれも写していない)。
Microsoft Learn の 「MIT (コード片)」 は、 文書のソースのリポジトリの表示からの推定 (公開ページに直接は書かれていない)。

| 番号 | 文献 | 使った段階 | コード |
| --- | --- | --- | --- |
| [1] | J. C. Hart, "Sphere Tracing: A Geometric Method for the Antialiased Ray Tracing of Implicit Surfaces", The Visual Computer 12, 1996. [doi:10.1007/s003710050084](https://doi.org/10.1007/s003710050084) | 1 | 論文 (コードは無い) |
| [2] | Inigo Quilez, "distance functions". [iquilezles.org/articles/distfunctions](https://iquilezles.org/articles/distfunctions) | 1 | コード片は MIT |
| [3] | Inigo Quilez, "normals for an SDF". [iquilezles.org/articles/normalsSDF](https://iquilezles.org/articles/normalsSDF) | 2 | コード片は MIT |
| [4] | Inigo Quilez, "raymarching SDFs". [iquilezles.org/articles/raymarchingdf](https://iquilezles.org/articles/raymarchingdf) | 1 | コード片は MIT |
| [5] | Inigo Quilez, "intersectors". [iquilezles.org/articles/intersectors](https://iquilezles.org/articles/intersectors) | 8, 9 | コード片は MIT |
| [6] | P. Gonzalez Vivo, J. Lowe, "The Book of Shaders" 第 10 章 ランダム. [thebookofshaders.com/10](https://thebookofshaders.com/10/?lan=jp) | 6 | 写さない (All rights reserved) |
| [7] | 同 第 11 章 ノイズ. [thebookofshaders.com/11](https://thebookofshaders.com/11/?lan=jp) | 6 | 写さない (All rights reserved) |
| [8] | 同 第 12 章 セルラーノイズ. [thebookofshaders.com/12](https://thebookofshaders.com/12/?lan=jp) | 8 | 写さない (All rights reserved) |
| [9] | J. de Vries, "LearnOpenGL" Cubemaps. [learnopengl.com/Advanced-OpenGL/Cubemaps](https://learnopengl.com/Advanced-OpenGL/Cubemaps) | 3〜12 | 写さない (CC BY-NC 4.0) |
| [10] | 同 PBR Theory. [learnopengl.com/PBR/Theory](https://learnopengl.com/PBR/Theory) | 3 | 写さない (CC BY-NC 4.0) |
| [11] | C. Schlick, "An Inexpensive BRDF Model for Physically-based Rendering", Computer Graphics Forum 13(3), 1994. [doi:10.1111/1467-8659.1330233](https://doi.org/10.1111/1467-8659.1330233) | 3 | 論文 (コードは無い) |
| [12] | M. Pharr, W. Jakob, G. Humphreys, "Physically Based Rendering" 第 4 版, "Specular Reflection and Transmission". [pbr-book.org](https://pbr-book.org/4ed/Reflection_Models/Specular_Reflection_and_Transmission) | 3, 4, 5 | 本文中のコードは写さない (CC BY-NC-ND 4.0) |
| [13] | 同 "Transmittance". [pbr-book.org](https://pbr-book.org/4ed/Volume_Scattering/Transmittance) | 6, 7, 10, 11 | 同上 |
| [14] | 同 "Volume Scattering Processes". [pbr-book.org](https://pbr-book.org/4ed/Volume_Scattering/Volume_Scattering_Processes) | 7, 10, 11 | 同上 |
| [15] | 同 "Phase Functions". [pbr-book.org](https://pbr-book.org/4ed/Volume_Scattering/Phase_Functions) | 10, 12 | 同上 |
| [16] | Wikipedia, "Total internal reflection". [en.wikipedia.org](https://en.wikipedia.org/wiki/Total_internal_reflection) | 5, 8, 9 | 本文 CC BY-SA 4.0 (写していない) |
| [17] | Wikipedia, "Ice cube" (Clear and cloudy). [en.wikipedia.org](https://en.wikipedia.org/wiki/Ice_cube) | 7 | 本文 CC BY-SA 4.0 (写していない) |
| [18] | S. G. Warren, R. E. Brandt, "Optical constants of ice from the ultraviolet to the microwave: A revised compilation", J. Geophys. Res. 113, 2008. [doi:10.1029/2007JD009744](https://doi.org/10.1029/2007JD009744) | 3 (屈折率 1.31) | 値は事実として使える |
| [19] | Wikipedia, "Bisection method". [en.wikipedia.org](https://en.wikipedia.org/wiki/Bisection_method) | 4 | 本文 CC BY-SA 4.0 (写していない) |
| [20] | F. Toft, J. Bowles, D. Zimmermann, "Optimisations for Real-Time Volumetric Cloudscapes", 2016. [arXiv:1609.05344](https://arxiv.org/abs/1609.05344) | 10 | 論文 (arXiv の利用許諾。 考え方だけ使った) |
| [21] | T. Sousa, "Generic Refraction Simulation", GPU Gems 2 第 19 章, 2005. [developer.nvidia.com](https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-19-generic-refraction-simulation) | 13 | 写さない (© NVIDIA、 All rights reserved) |
| [22] | Microsoft Learn, "refract" (HLSL). [learn.microsoft.com](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-refract) | 4, 5 | MIT (コード片) |
| [23] | Microsoft Learn, "Semantics" (SV_Depth). [learn.microsoft.com](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics) | 1 | MIT (コード片) |
| [24] | Microsoft Learn, "discard". [learn.microsoft.com](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/discard--sm4---asm-) | 1 | MIT (コード片) |
| [25] | Microsoft Learn, "ID3D12GraphicsCommandList::CopyResource". [learn.microsoft.com](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copyresource) | 13 | MIT (コード片) |
| [26] | Microsoft Learn, "Using Resource Barriers to Synchronize Resource States in Direct3D 12". [learn.microsoft.com](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12) | 13 | MIT (コード片) |
