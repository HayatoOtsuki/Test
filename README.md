# SDF Ice (DirectX 12)

SDF (符号付き距離場) で形を表した **透明な氷** を、 DirectX 12 で描くサンプル。
氷の部品 (`IceComponent` / `IceImplement` / `IceGenerate`) が、 キューブ・球・円柱のプリミティブに加えて、 `generateSDF` で作る SDF の氷を描く。

SDF の氷で出すもの:

- 浅い角度ほど強い映り込み (Fresnel)
- 屈折で向こうがずれて見える
- 厚い所ほど青い (吸収)
- 中から浅い角度で当たると出られない (全反射)
- 芯の白い濁り (気泡)
- 色付きの影

## 動かし方

必要なもの: Windows 10 / 11、 DirectX 12 が動く GPU、 Visual Studio 2022 (C++ によるデスクトップ開発)、 CMake 3.20 以上。

```bash
cmake -S . -B build
```

```bash
cmake --build build --config Debug
```

`build/Debug/SdfIce.exe` を起動する (Visual Studio で `build/SdfIce.sln` を開いて実行してもよい)。
シェーダーは実行時にソースのフォルダ (`src/engine/shaders`, `src/ice/shaders`) から読むので、 書き換えて R を押すと読み直せる。

機能ごとの簡易サンプル (プリミティブだけ・SDF だけ・レイマーチングだけ…) も同じビルドで `build/samples/Debug/` にできる。 一覧は [samples/README.md](samples/README.md)。

> このリポジトリは macOS で作った。 C++ は mingw-w64 で構文チェックとリンク、 HLSL は dxc でコンパイル、 シェーダーの絵は同じ HLSL を WebGL2 で動かして確かめたが、 **Windows での実行はまだ確かめていない**。

## 操作

| 入力 | 動作 |
| --- | --- |
| 左ドラッグ | カメラを回す |
| ホイール | 寄る・引く |
| 1 / 2 / 3 / 4、 Tab | 形を切り替える (キューブ / 球 / 円柱 / SDF の氷、 Tab は順に) |
| M | 金属と金属でないを切り替える (プリミティブ) |
| `[` `]` | 粗さを下げる・上げる (プリミティブ) |
| `,` `.` | 鏡面反射の強さを下げる・上げる (プリミティブ) |
| B | 気泡を入れる・抜く (SDF の氷) |
| J / L | 太陽の方角を回す |
| Space | カメラの自動回転を止める・動かす |
| R | シェーダーを読み直す |
| Esc | 終わる |

ウィンドウの題名に、 今の形とマテリアルの値が出る。

## フォルダの構成

```
src/
├── main.cpp
├── ice/                 … 氷の部品 (自分のエンジンへ持っていくのはここ)
│   ├── IceComponent     … 窓口。 RenderEntity を継承。 形・マテリアル・氷の値のセッター・ゲッター
│   ├── IceImplement     … 描く仕組み (initialize / draw3D / reloadShaders、 パイプライン、 mGeom)
│   ├── IceGenerate.cpp  … 形づくり (generateShape と、 キューブ・球・円柱・SDF)
│   └── shaders/         … IceMesh (プリミティブの PBR)、 IceSdf (SDF の氷)、 IceShadow (影)
└── engine/              … エンジンの代わり (氷を動かすための土台。 読まなくてよい)
samples/                 … 機能ごとの簡易サンプル (1 つの機能だけを小さく試す。 01〜05)
docs/
├── DESIGN.md            … 設計
├── 01〜08-*.md          … 要素ごとの解説 (コード・計算・アルゴリズム)
├── guide/               … 自分のプロジェクトに実装する手順 (schedule.txt から)、 参考文献と利用条件
└── presentation/        … 発表のスライドのテンプレートと構成のメモ
```

## 文書

| 読みたいこと | 文書 |
| --- | --- |
| 全体の設計 | [docs/DESIGN.md](docs/DESIGN.md) |
| 1 つの機能だけを動かして見る (プリミティブ / SDF / レイマーチング / SDF × レイマーチング / 焼いた SDF) | [samples/README.md](samples/README.md) |
| 自分のプロジェクトに足す手順 | [docs/guide/README.md](docs/guide/README.md)、 [docs/guide/schedule.txt](docs/guide/schedule.txt) |
| 部品の形 (Component / Implement / Generate) とプリミティブ、 PBR | [docs/03-ice-component.md](docs/03-ice-component.md) |
| カメラとライトの受け取り方 | [docs/04-camera-light.md](docs/04-camera-light.md) |
| SDF・generateSDF・レイマーチ | [docs/05-generate-sdf.md](docs/05-generate-sdf.md) |
| 反射・屈折・吸収・全反射 | [docs/06-ice-optics.md](docs/06-ice-optics.md) |
| 気泡 | [docs/07-ice-bubbles.md](docs/07-ice-bubbles.md) |
| 影 | [docs/08-ice-shadow.md](docs/08-ice-shadow.md) |
| エンジンの代わり (DirectX 12 の土台・背景) | [docs/01-dx12-base.md](docs/01-dx12-base.md)、 [docs/02-environment.md](docs/02-environment.md) |
| 参考文献と利用条件 | [docs/guide/references.md](docs/guide/references.md) |

## 自分のエンジンへ移すとき

`src/ice/` を持っていき、 エンジンとのつなぎ目を、 自分のエンジンの物に置き換える。

| つなぎ目 | このリポジトリ | 置き換える物 |
| --- | --- | --- |
| 描く物の基底クラスと登録 | `RenderEntity`、 `Renderer::addEntity` | エンジンの 「描く物」 の基底クラスと登録の仕組み |
| 描くときに渡される物 | `DrawContext` (コマンドリスト、 シーンの定数のアドレス) | エンジンの描画の文脈 |
| カメラとライト | `SceneConstants.hlsli` (b0) | エンジンのシーン (フレーム) の定数 ([docs/04-camera-light.md](docs/04-camera-light.md) §5) |
| 透けた先の景色 | `Environment.hlsli` の `Environment` | 空のキューブマップ、 または不透明な物の色のコピー ([docs/guide/step5-optics.md](docs/guide/step5-optics.md)) |
| シェーダーのコンパイル | `ShaderCompiler` (実行時に `D3DCompileFromFile`) | エンジンのシェーダーの仕組み |
| ベクトルと行列 | `MathTypes.h` (`Float3` / `Float4` / `Float4x4`) | エンジンの数学の型 |

## 調整できる値 (IceComponent)

| セッター | 範囲 | 既定 | 意味 |
| --- | --- | --- | --- |
| `setShape` | `IceShape` | `Cube` | 形 |
| `setPosition` (RenderEntity) | | (0, 0, 0) | 位置 (底の真ん中) |
| `setMaterial` / `setBaseColor` / `setMetallic` / `setRoughness` / `setSpecular` | 0〜1 (粗さは 0.04〜1) | 淡い水色、 0、 0.3、 0.5 | プリミティブの質感 |
| `setRefractiveIndex` | 1.0〜2.5 | 1.31 | SDF の氷の屈折率 |
| `setAbsorptionColor` | 各 0.01〜1 | (0.45, 0.75, 0.90) | 厚さ 1 m を通った白の色。 本物らしくするなら (0.76, 0.95, 0.997) |
| `setBubbleDensity` | 0〜50 | 5 | 芯の気泡の濃さ (1/m) |

## 開発の進め方

`master` (フェーズの完了) ← `develop` (統合) ← `feature/*` / `docs/*` (要素ごと)。 要素ごとに Issue と PR を作り、 フェーズはマイルストーンにまとめている。 詳しくは [docs/DESIGN.md](docs/DESIGN.md) の 「ブランチの運用」。
