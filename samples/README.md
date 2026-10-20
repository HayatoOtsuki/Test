# 機能ごとの簡易サンプル

本体 (`src/ice/`) は、 プリミティブ・SDF・レイマーチング・光学がすべて組み合わさっている。
ここには、 **1 つの機能だけ** を小さく試せるサンプルを置く。 どれもエンジンの代わり (`src/engine/`) をそのまま使い、 サンプルごとのコードは数百行に収めてある。

## 一覧と読む順

上から順に読むと、 「プリミティブ (表面だけの形)」 と 「SDF (中身まで持つ形)」 の違い、 そして SDF の氷の描き方までがつながる。

| サンプル | 試すこと | 見るとよい所 | キー |
| --- | --- | --- | --- |
| [01_primitives](01_primitives/README.md) | プリミティブだけ | 形は三角形の集まり。 持っているのは表面だけ | 1 / 2 / 3: 形、 W: ワイヤーフレーム、 N: 色の付け方 (光 / 法線 / UV) |
| [02_sdf](02_sdf/README.md) | SDF だけ | 空間のどの点にも 「表面までの距離」 がある。 中は負 | 1〜8: 形と組み合わせ、 ↑ ↓: 断面の位置、 P: 断面の向き |
| [03_raymarch](03_raymarch/README.md) | レイマーチングだけ | 光線を少しずつ進めて、 その場の値を足していく (SDF は使わない) | ↑ ↓: 歩数、 S: 太陽の影、 [ ]: 濃さ |
| [04_sdf_raymarch](04_sdf_raymarch/README.md) | SDF × レイマーチング | SDF の値だけ進めば表面を越えない (球追跡) | 1〜8: 形、 H: 表示 (光 / 歩数 / 法線)、 S: 影、 ↑ ↓: 歩幅の倍率 |
| [05_baked_sdf](05_baked_sdf/README.md) | SDF を焼いて描く | 式を 3D テクスチャに焼いても同じように描ける。 格子が粗いと角が丸まり、 細い所が消える | 1〜5: 格子の細かさ (8³〜128³)、 G: 格子の線 |
| [06_glass_ice](06_glass_ice/README.md) | 氷 (段階で作る) | 部品 1 つ = 氷 1 つ。 シェーダーは段階ごとのファイル IceShader0〜13 (気泡・亀裂・体積の散乱・画面のコピーの屈折)。 全部の式の解説と出典 | 0〜9・← →: 段階 |

どのサンプルでも同じキー: 左ドラッグ (カメラを回す)、 ホイール (寄る・引く)、 Space (自動回転)、 J / L (太陽を回す)、 R (シェーダーを読み直す)、 Esc (終わる)。
ウィンドウの題名に、 今の状態 (形・歩数など) が出る。

## 動かし方

リポジトリのいちばん上で、 本体と同じようにビルドする。

```bash
cmake -S . -B build
```

```bash
cmake --build build --config Debug
```

サンプルは `build/samples/Debug/` にできる (例: `Sample01Primitives.exe`)。 Visual Studio では、 ソリューションの `samples` フォルダにある。

> 本体と同じく、 このリポジトリは macOS で作った。 C++ は mingw-w64 で構文チェックとリンク、 HLSL は dxc でコンパイルして確かめたが、 Windows での実行はまだ確かめていない。

## フォルダの形

```
samples/
├── common/                  … 共通の土台 (サンプルの機能とは関係ない所)
│   ├── SampleApp            … 窓・カメラ・太陽・Renderer を持ち、 サンプル 1 つを回す
│   ├── SampleEntity.h       … サンプルの基底 (RenderEntity + キー + 題名)
│   ├── FullscreenPass       … 全画面の三角形で描くサンプル用のパイプライン
│   ├── GpuHelpers           … アップロードバッファ・ルートシグネチャ・PSO の決まった手順
│   └── shaders/             … SampleCommon.hlsli (シーンの定数・カメラの光線)、 Fullscreen.hlsli (全画面の頂点シェーダー)
└── 0N_xxx/
    ├── XxxSample.h / .cpp   … そのサンプルの本体 (ここを読む)
    ├── shaders/             … そのサンプルのシェーダー (ここを読む)
    ├── main.cpp             … SampleApp にサンプルを渡して回すだけ
    └── README.md            … 何を見るか・計算の解説
```

05 だけは、 焼く処理 (`BakedShape` / `SdfBaker`、 CPU だけ) と GPU へ送る処理 (`SdfTexture`) をクラスに分けてあるので、 ファイルが多い。

サンプル 1 つの `main.cpp` の大事な所はこれだけ (実際のファイルは、 これを失敗の表示 (try / catch) で包んでいる):

```cpp
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand)
{
    XxxSample sample(L"" SAMPLE_SHADER_DIR);       // サンプル (RenderEntity)
    SampleApp app(instance, L"0N サンプルの名前");  // 窓・カメラ・太陽・Renderer
    return app.run(sample, showCommand);           // 登録して、 窓が閉じるまで回す
}
```

## 本体との対応

| サンプル | 本体のどこに当たるか | 本体の解説 |
| --- | --- | --- |
| 01_primitives | `IceGenerate.cpp` の `generateCube` / `generateSphere` / `generateCylinder` | [docs/03-ice-component.md](../docs/03-ice-component.md) |
| 02_sdf | `IceGenerate.cpp` の `iceShapeSdf` (SDF の式と CSG) | [docs/05-generate-sdf.md](../docs/05-generate-sdf.md) §1〜2 |
| 03_raymarch | `IceSdf.hlsl` の `MarchInside` (中を進んで吸収と気泡を足す所) | [docs/07-ice-bubbles.md](../docs/07-ice-bubbles.md) |
| 04_sdf_raymarch | `IceSdfCommon.hlsli` の `SphereTrace` / `SdfNormal` | [docs/05-generate-sdf.md](../docs/05-generate-sdf.md) §5 |
| 05_baked_sdf | `IceGenerate.cpp` の `generateSDF` と `IceSdf.hlsl` の形を探す所 | [docs/05-generate-sdf.md](../docs/05-generate-sdf.md) §3〜5 |
| 06_glass_ice | `IceSdf.hlsl` の `ShadeIceSdf` を、 部品 1 つ = 形 1 つ で書き直した物 (映り込み・屈折・全反射・吸収・芯) | [docs/06-ice-optics.md](../docs/06-ice-optics.md)、 [06 の README](06_glass_ice/README.md) |
