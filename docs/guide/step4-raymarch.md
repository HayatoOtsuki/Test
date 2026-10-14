# step4. 箱を描いて、 箱の中をレイマーチする (まずは不透明)

**ゴール**: SDF の形が画面に出る。 この段では、 当たった所を **今の PBR のシェーダーで不透明に塗る**。 形と法線が正しいことを先に確かめ、 透明にするのは step5。

計算の解説: [`docs/05-generate-sdf.md`](../05-generate-sdf.md) §5。 見本: `src/ice/IceGenerate.cpp` (`generateSDF` の 7、 `appendBox`)、 `src/ice/IceImplement.cpp` (`buildPipelines`)、 `src/ice/shaders/IceSdfCommon.hlsli`, `IceSdf.hlsl`。

---

## 1. 箱のメッシュ

格子が覆う箱 (`boundsMin`〜`boundsMax`) の立方体を作り、 SDF の形の頂点・インデックスにする。
プリミティブのキューブを作る関数を、 「中心と半分の大きさ」 を受け取る形にして使い回すとよい (見本: `appendBox`)。

形の構造体に、 SDF の形のときだけ使う物を足す:

| 足す物 | 使い道 |
| --- | --- |
| `isSdf` | 描くときにパイプラインを選ぶ |
| 3D テクスチャ、 アップロードバッファ | step3 |
| `boundsMin`, `boundsMax`, `voxelSize` | シェーダーへ渡す (箱の範囲、 法線の差分の幅) |

## 2. SDF 用のパイプライン

プリミティブ用と同じ入力レイアウトで、 次だけ変える。

| 設定 | 値 | 理由 |
| --- | --- | --- |
| カリング | **表面をカリング** (`D3D12_CULL_MODE_FRONT`) | 裏面を描けば、 カメラが箱の中でも画素が出る |
| 深度の書き込み | しない (`D3D12_DEPTH_WRITE_MASK_ZERO`) | 箱の面の深さは、 氷の表面の深さではない |
| シェーダー | SDF 用 (新しいファイル) | |

> 三角形の向き (表が反時計回りか時計回りか) は、 プリミティブのパイプラインと同じ約束にそろえる。 違うと、 表面だけが描かれて、 寄ると消える。

`draw3D` で、 形が SDF ならこのパイプラインと SRV のテーブルを設定する。

## 3. シェーダー

定数に `BoundsMin` / `BoundsMax` / `VoxelSize` / 氷の位置を足し、 次の関数を順に書く (見本: `IceSdfCommon.hlsli`)。

| 関数 | すること |
| --- | --- |
| `SampleSdf(p)` | 物体の空間の点を 0〜1 の `uvw` に直して 3D テクスチャを読む。 箱の外は箱までの距離を足す |
| `SdfNormal(p)` | 各軸に格子 1 つ分ずらした差 (中心差分) を正規化 |
| `IntersectBox(ro, rd, …)` | スラブ法で、 光線が箱を通る区間 `[tNear, tFar]` |
| `SphereTrace(ro, rd, …)` | φ × 0.9 ずつ進み、 φ < 0.5 mm で当たり。 128 回で打ち切り |

ピクセルシェーダー:

```hlsl
float3 ro = CameraPos - Position;               // 物体の空間のカメラ (step1 のカメラの位置を使う)
float3 rd = normalize(worldPos - CameraPos);     // この画素の光線
if (!IntersectBox(ro, rd, tNear, tFar)) discard;
if (!SphereTrace(ro, rd, max(tNear, 0.0), tFar, tHit)) discard;
float3 p = ro + rd * tHit;
return 今の PBR(p + Position, SdfNormal(p));       // この段は不透明
```

- `SampleLevel(…, 0)` を使う (ループの中では `Sample` が使えない)。
- 氷を回したいときは、 ワールド → 物体の空間の行列で `ro` と `rd` を変える (このリポジトリは回さないので位置を引くだけ)。

## 確かめ方

- [ ] SDF に切り替えると、 角の丸い塊が出る
- [ ] 箱の形 (塊の周りの四角) が見えない
- [ ] 太陽の側が明るく、 反対が暗い (法線が外を向いている)
- [ ] 寄っても、 回り込んでも消えない
- [ ] 表面に穴・点・筋が無い

## つまずきやすい所

| 症状 | 原因 |
| --- | --- |
| 何も出ない | SRV のテーブルを設定していない / `discard` の条件が逆 / カメラの位置が step1 で来ていない |
| 箱の四角が塗られる | 当たらなかった画素で `discard` していない |
| 塊が箱の中でずれる | `uvw` の計算と、 焼いたときの箱 (`boundsMin`, `boundsMax`) が違う |
| 寄ると消える | 表面をカリングしていない (裏面が消えている) |
| 光の当たり方が逆 | 法線の向き (差の順) が逆 |
| 縁にぎざぎざ | 球追跡の回数が足りない (128 に上げる) / 当たりとみなす距離が大きい |
