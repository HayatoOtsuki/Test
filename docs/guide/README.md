# 実装の手順: 自分のプロジェクトに SDF の氷を足す

このリポジトリを見ながら、 自分のプロジェクトに SDF の氷を **1 段ずつ** 実装するための手順。
進め方の一覧 (チェックリスト) は [`schedule.txt`](schedule.txt)。

## 前提 (今の自分のプロジェクト)

- `IceComponent` / `IceGenerate` / `IceImplement` がある
- キューブ・スフィア・円柱のプリミティブを作って `mGeom` に入れ、 切り替えて描ける
- シェーダーでマテリアル (基本色・メタリック・ラフネス・スペキュラ) を調整できる
- カメラとディレクショナルライトの値は、 シェーダーにハードコードしている

このリポジトリは同じ形 (`IceComponent : RenderEntity`、 `IceImplement`、 `IceGenerate.cpp` の `generateShape`) で作ってあるので、 ファイルと関数の名前で見比べられる。
`src/engine/` は自分のエンジンの代わりなので、 読まなくてよい (つなぎ目だけ各手順で説明する)。

## 読む順

| 段 | 手順 | やること | 自分のプロジェクトで触る所 | このリポジトリの見本 | 計算の解説 |
| --- | --- | --- | --- | --- | --- |
| 1 | [step1](step1-camera-light.md) | カメラとライトを定数で渡す | 氷のシェーダー、 `IceImplement` のルートシグネチャと `draw3D` | `src/engine/SceneConstants.h`, `shaders/SceneConstants.hlsli` | [04](../04-camera-light.md) |
| 2 | [step2](step2-bake-sdf.md) | CPU で SDF を焼く | 形の種類、 `IceGenerate.cpp` (`generateShape`, `generateSDF`) | `src/ice/IceGenerate.cpp` | [05](../05-generate-sdf.md) §1〜3 |
| 3 | [step3](step3-sdf-texture.md) | 3D テクスチャで GPU へ送る | `generateSDF`、 `IceImplement` (ルートシグネチャ・ヒープ) | `IceGenerate.cpp` (`generateSDF` の 3〜6)、 `IceImplement.cpp` | [05](../05-generate-sdf.md) §4 |
| 4 | [step4](step4-raymarch.md) | 箱の中をレイマーチ (不透明) | 箱のメッシュ、 SDF のパイプライン、 新しいシェーダー | `IceSdfCommon.hlsli`, `IceSdf.hlsl` | [05](../05-generate-sdf.md) §5 |
| 5 | [step5](step5-optics.md) | 反射・屈折・吸収・全反射 | SDF のシェーダー、 セッター | `IceSdf.hlsl` (`ShadeIceSdf`, `MarchInside`) | [06](../06-ice-optics.md) |
| 6 | [step6](step6-bubbles.md) | 気泡の濁り | SDF のシェーダー、 セッター | `IceSdfCommon.hlsli` (`BubbleDensityAt`), `IceSdf.hlsl` | [07](../07-ice-bubbles.md) |
| 7 | [step7](step7-shadow.md) | 色付きの影 | 影の描き方 (エンジンによる) | `IceShadow.hlsl` | [08](../08-ice-shadow.md) |
| 8 | [step8](step8-debug-tune.md) | デバッグと調整 | どこでも | — | — |

### 先に動かすとよいサンプル

各段の前に、 その段の機能だけを取り出したサンプル ([`samples/`](../../samples/README.md)) を動かすと、 何を作るのかが目で分かる。

| 段 | サンプル | 見ておくこと |
| --- | --- | --- |
| 2 | [02_sdf](../../samples/02_sdf/README.md) | SDF は空間の全部の点の 「表面までの距離」。 中が負。 断面を動かして形の中を見る |
| 2〜3 | [05_baked_sdf](../../samples/05_baked_sdf/README.md) | 焼いて 3D テクスチャにする流れ。 格子の細かさで形がどう崩れるか |
| 4 | [04_sdf_raymarch](../../samples/04_sdf_raymarch/README.md) | 球追跡で表面を探す。 歩数・法線・歩幅を越えたときの崩れ方 |
| 5〜6 | [03_raymarch](../../samples/03_raymarch/README.md) | 中を少しずつ進んで吸収と散乱を足す (`MarchInside` と同じ形) |
| (今の状態) | [01_primitives](../../samples/01_primitives/README.md) | 今のプリミティブとの違い (表面しか持たない) |

参考文献と、 その利用条件 (正規の入手先・引用と実装・コードの流用・商用利用) は [`references.md`](references.md)。

## 進め方の約束

1. **1 段ずつ、 確かめてからコミット**。 段の終わりの 「確かめ方」 を全部見てから次へ進む。
2. **1 段で 1 つのことだけ変える**。 絵が壊れたとき、 直前に変えた所が原因だとすぐ分かる。
3. **見本は写さず、 読んでから自分で書く**。 このリポジトリのコードは独自に書いたものなので写しても問題は無いが、 自分のエンジンの関数・型・決まり (行列の向き、 ライトの向きの約束、 ヒープの扱い) に合わせて書き直さないと動かない。
4. **数を出して確かめる**。 「見た目がそれっぽい」 だけで進まない。 SDF の値、 定数バッファの中身、 テクスチャの中身を、 ログや RenderDoc で数字で見る。

## このリポジトリの動かし方 (見本として)

Windows で:

```bash
cmake -S . -B build
```

```bash
cmake --build build --config Debug
```

`build/Debug/SdfIce.exe` を起動する。 操作は `docs/DESIGN.md` の 「操作」。 4 で SDF の氷、 B で気泡、 J / L で太陽、 R でシェーダーの読み直し。

## 用語

| 用語 | 意味 |
| --- | --- |
| SDF (符号付き距離場) | 空間の各点に 「形の表面までのいちばん近い距離」 を持たせたもの。 外が正、 中が負 |
| 球追跡 (sphere tracing) | SDF の値だけ光線を進めることを繰り返して、 表面を探す方法 |
| レイマーチ | 光線を少しずつ進めながら調べること。 球追跡はその 1 つ |
| 格子 / ボクセル | 空間を細かい立方体のマス目に分けたもの。 その 1 つがボクセル |
| 3D テクスチャ | 幅・高さ・奥行きのある画像。 格子の値を入れ、 シェーダーから補間して読む |
| SRV | シェーダーからリソースを読むための記述子 (どの形式・どの次元で読むか) |
| デスクリプターヒープ / テーブル | 記述子を並べて置く場所 / その場所を指すルート引数 |
| ルート定数 / CBV | ルート引数に直接置く小さな定数 / 定数バッファを GPU のアドレスで指すルート引数 |
| Fresnel | 表面で反射する割合が、 見る角度で変わること。 浅い角度ほど強く反射する |
| F0 | 正面から見たときの反射率。 屈折率から決まる (氷は約 0.018) |
| 屈折率 | 光が物の中でどれだけ遅くなるか。 曲がり方を決める (氷は 1.31) |
| 全反射 | 屈折率の高い所から低い所へ、 浅い角度で当たった光が全部跳ね返ること (氷は約 49.8° より浅いと起きる) |
| Beer–Lambert の法則 | 物の中を進む光が、 距離に対して指数関数で減ること |
| 散乱 | 光が小さな粒 (気泡) に当たって、 いろいろな向きへ散らばること |
| 光学的な厚さ | 密度 × 長さの合計。 その分だけ光が `exp(−厚さ)` に減る |
| トーンマップ | 明るさ (上限なし) を画面の色 (0〜1) に直すこと |

## 行き詰まったら

[`step8-debug-tune.md`](step8-debug-tune.md) の 「デバッグ表示」 と 「症状から探す」 を見る。 各手順の 「つまずきやすい所」 にも、 よくある症状と原因を書いてある。
