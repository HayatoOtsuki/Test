# step2. generateSDF: CPU で SDF を焼く

**ゴール**: `generateShape` の for と switch に SDF の形を足し、 CPU で氷の形の SDF を 64³ の格子に焼く。
この段では GPU には送らない。 **焼いた値が正しいことを、 数字で確かめる** ところまで。

計算の解説: [`docs/05-generate-sdf.md`](../05-generate-sdf.md) §1〜3。 見本: `src/ice/IceGenerate.cpp` (`sdRoundBox`, `sdSphere`, `smoothMax`, `valueNoise`, `iceShapeSdf`, `generateSDF` の 1〜2)。

---

## 1. 形の種類を足す

1. 形の種類の列挙に `Sdf` を足す。 **`Count` の前** に置く (for で全部の形を回すため)。

```cpp
enum class IceShape {
    Cube,
    Sphere,
    Cylinder,
    Sdf,    // ← 足す
    Count
};
```

2. `generateShape` の switch に case を足す。 SDF は後の段でテクスチャを送る命令を記録するので、 **コマンドリストを渡せる形** にしておく。

```cpp
case IceShape::Sdf: mGeom[i] = generateSDF(commandList); break;
```

> 自分のプロジェクトの `generateShape` が `draw3D` の中で呼ばれていれば、 `draw3D` のコマンドリストをそのまま渡せる。
> 別の所 (初期化) で呼んでいる場合は、 エンジンの 「初期化のときに GPU へ送る」 仕組み (アップロード用のコマンドリスト) を探す (step3)。

3. 切り替えのキーや UI に SDF を足す。 この段では、 `generateSDF` は空の形 (または今のキューブ) を返すだけでよい。

## 2. 距離の関数を書く

まずは **角を丸めた箱だけ**。 ゆらぎと欠けは、 光学 (step5) まで済んでから足す (schedule.txt の 6)。

```cpp
float sdRoundBox(Float3 p, Float3 half, float r)
{
    const Float3 q = { std::fabs(p.x) - (half.x - r), std::fabs(p.y) - (half.y - r), std::fabs(p.z) - (half.z - r) };
    const Float3 outside = { std::max(q.x, 0.0f), std::max(q.y, 0.0f), std::max(q.z, 0.0f) };
    const float inside = std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
    return std::sqrt(dot(outside, outside)) + inside - r;
}
```

式の意味は [`docs/05-generate-sdf.md`](../05-generate-sdf.md) §2.1。 書けたら、 **焼く前に** 値を確かめる:

| 点 | 期待する値 (半分の大きさ 0.38、 丸み 0.08、 中心 (0, 0.38, 0)) |
| --- | --- |
| 中心 (0, 0.38, 0) | 約 −0.38 (中) |
| 横に 0.6 (0.6, 0.38, 0) | 約 +0.22 (外。 面までの距離) |
| 底の真ん中 (0, 0, 0) | 約 0 (表面) |
| 角の外 (0.5, 0.88, 0.5) | 正 (外。 丸めた角までの距離) |

```cpp
// 例: 起動時に 1 回だけログに出す
printf("center %f side %f bottom %f\n", iceShapeSdf({0, 0.38f, 0}), iceShapeSdf({0.6f, 0.38f, 0}), iceShapeSdf({0, 0, 0}));
```

## 3. 格子に焼く

```cpp
const float half = kIceHalfSize + kSdfMargin;              // 氷 + 余白
const Float3 boundsMin = { -half, kIceHalfSize - half, -half };
const float voxel = (2.0f * half) / n;                      // n = 64
for (z) for (y) for (x) {
    const Float3 p = { boundsMin.x + (x + 0.5f) * voxel, ... };
    grid[(z * n + y) * n + x] = iceShapeSdf(p);
}
```

| 決めること | 理由 |
| --- | --- |
| 余白 (6 cm) を足す | 表面の外にも距離の値が要る。 格子の外周の 1 層がすべて正 (外) になるか確かめる |
| `x + 0.5` (マス目の真ん中) | 3D テクスチャを線形補間で読むときの、 テクセルの中心と合わせる |
| x → y → z の並び | 3D テクスチャのメモリの並びと同じにする (step3 で 1 行ずつ写せる) |

確かめる: 格子の最小値 (約 −0.37) と最大値 (正)、 外周の 1 層がすべて正。

## 4. (後で) 表面のゆらぎと角の欠け

光学 (step5) が動いてから足すと、 「ゆらぎで壊れたのか、 光学で壊れたのか」 を分けられる。

1. **角の欠け**: 球の SDF を作り、 なめらかな最大で削る。 `d = smoothMax(d, -sdSphere(p, c, r), 0.05)`
2. **ゆらぎ**: 値ノイズを 2 つ (周波数 9 と 23) 重ね、 6 mm の大きさで距離に足す
3. 確かめる: 欠けた角の点が正 (外) になる。 屈折の模様がゆがむ

式とアルゴリズム (CSG、 なめらかな最大、 値ノイズ) は [`docs/05-generate-sdf.md`](../05-generate-sdf.md) §2.3〜2.4。

## 確かめ方

- [ ] 形の切り替えに SDF が増え、 選ぶと (まだ何も描かれないか、 仮の形が描かれる)
- [ ] 中心・横・底の SDF の値が、 上の表と合う
- [ ] 格子の外周の 1 層がすべて正
- [ ] 焼く時間が気にならない (64³ なら一瞬。 遅ければリリースビルドで測る)

## つまずきやすい所

| 症状 | 原因 |
| --- | --- |
| 中心が正になる | 箱の中心をずらし忘れ (`p - center`)、 または `inside` の min / max が逆 |
| 外周に負の値がある | 余白が足りない、 または格子の箱の位置が氷とずれている |
| 値が全部同じ | 格子の点の位置の計算で、 整数の割り算になっている (`x / n` が 0) |
