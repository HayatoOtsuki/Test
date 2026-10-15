# 03. レイマーチングだけ

**レイマーチング** そのものを見るサンプル。 画素ごとに光線を飛ばし、 **一定の歩幅で少しずつ進めながら、 その場の値を読んで足していく**。
描くのは **霧の玉**。 霧には表面が無いので、 SDF (表面までの距離) は使わない。 使うのは 「その点の霧の濃さ」 だけ。

| キー | 動作 |
| --- | --- |
| ↑ ↓ | 歩数を 2 倍 / 半分 (2〜256) |
| S | 影: 各点から太陽へ 2 回目のレイマーチをする / しない |
| [ ] | 霧の濃さ (0〜30 /m) |

タイトルバーに、 1 画素で霧の濃さを何回読んだか (= 重さの目安) が出る。

試すこと:

1. ↓ で歩数を 2 まで下げる。 玉の縁が欠け、 むらの形も変わる。 歩幅 (玉の直径 ÷ 歩数 = 50 cm) が、 むらの大きさ (10〜25 cm) より粗いので、 むらを読み飛ばしている。
2. ↑ で 8 → 32 → 128 と上げる。 8 くらいから見た目がほとんど変わらなくなる。 **歩数は 「見た目が変わらなくなる所」 まであれば十分** で、 それ以上は重いだけ。
3. S で影を切る。 太陽の反対側 (カメラ側の下の方) が明るくなる。 影ありは、 1 画素の読み出しが 9 倍 (1 + 8) に増える。
4. ] で 25 くらいまで濃くする。 向こうの床が見えなくなり、 太陽に向いた左上だけが明るく、 残りは空の光だけの青っぽい灰色になる (太陽の光が奥まで届かない)。 雲が立体的に見えるのはこのため。 [ で 1 まで薄くすると、 ほとんど透明になる。

## 読む順

1. [`shaders/RaymarchFog.hlsl`](shaders/RaymarchFog.hlsl) — `ShadeFog` (レイマーチの本体)、 `FogDensity` (霧の濃さ)、 `SunVisibility` (影)
2. [`RaymarchSample.cpp`](RaymarchSample.cpp) — キーと定数 (全画面で描く部分は `common/FullscreenPass`)

## レイマーチングとは

光線 $\mathbf{p}(t) = \mathbf{o} + t\,\mathbf{d}$ の上を、 $t$ を少しずつ増やしながら点を取り、 その点で何かを計算すること。

```hlsl
float t = start;
for (int i = 0; i < N; ++i) {
    float3 p = ro + rd * t;  // 今いる点
    // p で何かを読む (霧の濃さ、 SDF、 3D テクスチャ…)
    t += step;               // 次へ
}
```

**何を読むか** と **歩幅をどう決めるか** で、 名前と用途が変わる。

| | 読む物 | 歩幅 | 止まる所 | このリポジトリ |
| --- | --- | --- | --- | --- |
| このサンプル | 霧の濃さ | 一定 | 玉の出口 | 03 |
| 球追跡 (sphere tracing) | SDF | その場の SDF の値 | 表面 ($\phi \approx 0$) | 04 / 05 / 本体の `SphereTrace` |
| 本体の `MarchInside` | SDF と気泡の濃さ | SDF から決める (上限 5 cm) | 氷の出口 ($\phi > 0$) | `IceSdf.hlsl` |

霧のように **中身がずっと続く物** は、 表面を探しても意味が無い。 中を少しずつ進んで、 通った所の分を全部足す。 これがこのサンプルの形。

## 1. 霧の玉を通る区間を求める

全画面のどの画素でも全部の歩を進めると無駄なので、 まず光線が玉 (中心 $\mathbf{c}$、 半径 $R$) を通る区間 $[t_0, t_1]$ を求める。

$$\lVert \mathbf{o} + t\mathbf{d} - \mathbf{c} \rVert^2 = R^2$$

を $t$ について解く ($\lVert\mathbf{d}\rVert = 1$)。 $\mathbf{oc} = \mathbf{o} - \mathbf{c}$、 $b = \mathbf{oc}\cdot\mathbf{d}$、 $c = \mathbf{oc}\cdot\mathbf{oc} - R^2$ とおくと

$$t^2 + 2bt + c = 0 \quad\Rightarrow\quad t = -b \pm \sqrt{b^2 - c}$$

```hlsl
float h = b * b - c;
if (h < 0.0) return false;  // 根号の中が負 = 玉に当たらない
h = sqrt(h);
t0 = max(-b - h, 0.0);      // 入口 (カメラが玉の中なら、 入口はカメラ)
t1 = -b + h;                // 出口
```

## 2. 一定の歩幅で進む

```hlsl
float dt = 2.0 * FOG_RADIUS / float(StepCount);
float t = (floor(t0 / dt) + 0.5) * dt;
```

- 歩幅 `dt` = 玉の直径 ÷ 歩数。 玉をまっすぐ通る光線が、 ちょうど `StepCount` 歩で抜ける。
- 最初の歩は、 入口 `t0` の手前の 「`dt` の倍数 + 半歩」。 こうすると、 どの画素でも 「カメラからの距離が同じ所」 で霧を読むことになる。 歩数が少ないと、 カメラを中心にした玉ねぎの皮のような層が見えることがある (歩幅の跡)。 入口の手前の点は霧の外なので、 濃さは 0 で何も足さない。

## 3. 1 歩ごとに足す (吸収と散乱)

各歩で、 その点の濃さ $\sigma$ (1 m 進む間に光が霧に当たる度合い、 単位 1/m) を読む。

```hlsl
float a = 1.0 - exp(-sigma * dt);        // この 1 歩で霧に当たる割合
scattered += transmittance * a * light;  // 当たった光の一部が目の方へ散らばる
transmittance *= 1.0 - a;                // 当たった分、 奥からの光は減る
```

### 1 歩で霧に当たる割合

濃さ $\sigma$ が一定の所を長さ $\Delta t$ 進むと、 光が霧に当たらずに抜ける割合は **Beer–Lambert の法則**:

$$T = e^{-\sigma \Delta t}$$

なので、 当たる割合は $a = 1 - e^{-\sigma\Delta t}$。 $\sigma\Delta t$ が小さいときは $a \approx \sigma\Delta t$ (濃さ × 長さ) と同じだが、 歩幅が大きくても $a$ が 1 を超えないのが良い所。

### 足し方

- `transmittance` ($T$): **ここまでの霧に遮られずに残っている割合**。 1 から始まり、 歩くたびに $(1-a)$ を掛けて減る。
- `scattered`: **霧が目の方へ散らばらせた光**。 その歩で当たった割合 $a$ × その点を照らす光 × **そこから目まで届く割合 $T$**。 手前に霧が多いほど、 奥の霧の光は届きにくい。

全部進んだら、 霧の向こうの景色 (空と床) が残った割合だけ見える:

$$\text{色} = \text{scattered} + T \times \text{背景}$$

これは、 本当は積分 $\int_{t_0}^{t_1} T(t)\,\sigma(t)\,L(t)\,dt$ で表される量を、 歩数ぶんの足し算で近づけたもの。 歩数が多いほど本当の値に近い。

### 途中でやめる

```hlsl
if (transmittance < 0.01) break;
```

残りが 1% を切ったら、 その先の霧も背景もほとんど見えないので読まない (よく使う省き方)。

## 4. 影 = 2 回目のレイマーチ

各点を照らす太陽の光は、 太陽までの間の霧で減る。 各点から太陽の方向へ、 もう一度レイマーチして **光学的な厚さ** (濃さ × 長さ の合計) を求める:

$$\tau = \sum_j \sigma(\mathbf{p} + t_j \mathbf{L})\,\Delta t, \qquad \text{届く割合} = e^{-\tau}$$

```hlsl
for (int i = 0; i < SHADOW_STEPS; ++i) {
    opticalDepth += FogDensity(p + L * (t0 + (float(i) + 0.5) * dt)) * dt;
}
return exp(-opticalDepth);
```

- 1 回目の各歩で 2 回目を 8 歩するので、 読み出しは $N \times (1 + 8)$ 回。 **入れ子のレイマーチは重い**。 影の歩数を少なくしているのはそのため。
- 散らばる向きの偏り (前へ散りやすい等) は入れず、 どの向きにも同じだけ散る、 として簡単にしている。

## 5. 霧の濃さ

```hlsl
float r = length(p - FOG_CENTER) / FOG_RADIUS;  // 0 (真ん中) 〜 1 (縁)
float ball = 1.0 - smoothstep(0.8, 1.0, r);     // 縁の外側 2 割で 1 → 0
float noise = 0.5 * ValueNoise(p * 4.0 + flow) + 0.25 * ValueNoise(p * 10.0 + flow);
return Density * ball * saturate(0.55 + noise);
```

- 玉の形: 中ではほぼ一定で、 縁の近くでなめらかに 0 にする (縁でぷつっと切れると、 輪郭がぎざぎざになる)。
- ノイズ: 大きなむら (周期 25 cm くらい) と細かいむら (10 cm くらい) を足す。 `flow` で時間とともに上へ流す。
- 値ノイズは 02 の `SdfShapes.hlsli` と同じ物。

**レイマーチングで読む物は、 式でも 3D テクスチャでも良い**。 ここを 「SDF から決まる気泡の濃さ」 にすると、 本体の氷の気泡になる。

## 本体の氷との対応

本体の [`IceSdf.hlsl`](../../src/ice/shaders/IceSdf.hlsl) の `MarchInside` は、 このサンプルと同じ形をしている。

| このサンプル | 本体の `MarchInside` |
| --- | --- |
| `FogDensity(p)` | `BubbleDensityAt(q, -d)` (表面からの深さで決まる気泡の濃さ) |
| `a = 1 - exp(-sigma * dt)` | `a = 1 - exp(-BubbleDensityAt(...) * s)` |
| `scattered += transmittance * a * light` | `col += thr * exp(-Absorption * t) * (bubbleT * a) * BubbleLight()` |
| `transmittance *= 1 - a` | `bubbleT *= 1 - a` |
| 一定の歩幅 `dt` | SDF から決めた歩幅 `clamp(-d, VoxelSize * 0.5, 0.05)` |
| 玉の出口 `t1` で止まる | SDF が正 (外) になったら止まり、 2 分探索で出口を詰める |

違いは、 **本体は 「どこまでが中か」 を SDF で知っている** こと。 霧の玉は球の式で区間を出したが、 氷は形が複雑なので SDF を読みながら外へ出た所で止まる。 これが 04 (SDF × レイマーチング) につながる。

## 関連

- [docs/07-ice-bubbles.md](../../docs/07-ice-bubbles.md) — 気泡の濃さと散乱
- [docs/06-ice-optics.md](../../docs/06-ice-optics.md) — Beer–Lambert の吸収
