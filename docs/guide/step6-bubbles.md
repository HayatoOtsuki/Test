# step6. 気泡 (芯の白い濁り)

**ゴール**: 冷凍庫の氷のように芯が白く濁る。 気泡の密度 0 で澄んだ氷に戻る。

計算の解説: [`docs/07-ice-bubbles.md`](../07-ice-bubbles.md)。 見本: `src/ice/shaders/IceSdfCommon.hlsli` (`BubbleDensityAt`)、 `IceSdf.hlsl` (`MarchInside`, `BubbleLight`)。

---

## 1. 気泡の密度を、 SDF の深さで決める

中では $-\phi$ が表面からの深さ。 **深い所 (芯) だけ** に気泡を置く。

```hlsl
float BubbleDensityAt(float3 p, float depth)
{
    float core = smoothstep(0.10, 0.22, depth);   // 10 cm より浅い所は 0、 22 cm より深い所は 1
    float pattern = 0.55 + 0.45 * sin(p.x * 37.0) * sin(p.y * 41.0 + 1.7) * sin(p.z * 39.0 + 0.6);
    return BubbleDensity * core * pattern;
}
```

- `smoothstep` の 2 つの値は、 氷の大きさに合わせて決める (小さい氷なら浅くする)。
- 模様は濁りにむらを付けるため。 値ノイズでもよい (step2 のゆらぎと同じ作り方)。

## 2. 中を進むときに、 散乱を足して減らす

`MarchInside` の 1 歩 (長さ `s`) ごとに:

```hlsl
float a = 1.0 - exp(-BubbleDensityAt(q, -d) * s);                // この 1 歩で気泡に当たる割合
col += thr * exp(-Absorption * t) * (bubbleT * a) * BubbleLight(); // 散らばって目に届く光
bubbleT *= 1.0 - a;                                                // 気泡に遮られずに残る割合
```

出口に着いたら、 吸収と気泡の分をまとめて減らす。

```hlsl
thr *= exp(-Absorption * t) * bubbleT;
```

そのために、 `MarchInside` に `col` と `thr` を `inout` で渡す形に変える (step5 では吸収だけだった)。

## 3. 気泡を照らす光

```hlsl
float3 BubbleLight() { return SunColor() * 0.25 + float3(0.35, 0.42, 0.55); }
```

周りからぼんやり届く一定の光とみなす近似。 自分のエンジンに環境光 (アンビエント) の値や、 空のキューブマップの平均の色があれば、 それを使うと周りとなじむ。

## 4. セッター・ゲッター

`setBubbleDensity` / `getBubbleDensity` (0〜50、 既定 5)。 定数に 1 つ足す。

## 確かめ方

- [ ] 芯が白く濁り、 外側 10 cm くらいは澄んでいる
- [ ] 濁りにむらがある
- [ ] 芯の向こうの景色が見えにくい
- [ ] 密度 0 で澄んだ氷になる

## つまずきやすい所

| 症状 | 原因 |
| --- | --- |
| 全体が白い | `smoothstep` の範囲が表面から始まっている |
| 縞々になる | 1 歩が大きい (中を進む上限 5 cm を下げる) |
| 濁りが灰色 | `BubbleLight` が弱い / 吸収を 2 回掛けている |
| 濁りが明るすぎる | 散乱に `bubbleT` を掛け忘れ (手前の気泡で遮られた分) |
