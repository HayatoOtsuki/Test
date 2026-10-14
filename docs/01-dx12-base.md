# 01. DirectX 12 の土台 (エンジンの代わり)

氷の部品 (`src/ice/`) を動かすための、 最小のエンジンの代わりを `src/engine/` に作る。
自分のエンジンへ氷の部品を移すときは不要な部分なので、 **氷の部品とのつなぎ目 (`DrawContext` と行列の約束)** だけ押さえればよい。

この段の絵: 空のグラデーションと、 地平線より下の仮の地面。 ドラッグで向きが変わる。

## ファイル

| ファイル | 役目 |
| --- | --- |
| `src/main.cpp` | 入口。 `Application` を作って回し、 例外をメッセージボックスで見せる |
| `Application` | 部品をつなぐ。 入力 → カメラ、 毎フレーム `beginFrame` → (部品の `draw3D`) → `endFrame` |
| `Window` | ウィンドウとメッセージ。 入力を関数 (コールバック) で知らせる |
| `OrbitCamera` | 角度と距離。 位置・ビュー行列・射影行列を作る |
| `GraphicsDevice` | デバイス・コマンドキュー・フェンス (`waitForGpu`) |
| `SwapChain` / `DepthBuffer` | バックバッファ 2 枚と深度バッファ |
| `Renderer` | フレームの始まり (クリア・背景) と終わり (実行・表示・待つ) |
| `BackgroundPass` | 空と床を全画面の三角形で描く |
| `ShaderCompiler` | HLSL のファイルを実行時にコンパイル (R キーで読み直し) |
| `MathTypes.h` | `Float3` / `Float4x4` とカメラの行列 |
| `DrawContext.h` | 部品の `draw3D` へ渡す物 |

## 1 フレームの流れ

`Application::run` のループの 1 周が 1 フレーム。

```cpp
while (mWindow.processMessages()) {
    // ... 経過時間、 シェーダーの読み直し、 大きさの変更、 カメラの更新 ...
    mRenderer.beginFrame(mCamera, time);  // 背景まで描く
    mRenderer.endFrame();
}
```

- `processMessages` はたまっている入力を全部処理し、 ウィンドウが閉じたら `false` を返す。
- `beginFrame` と `endFrame` の間に、 次の段 (`feature/ice-component`) で `IceComponent::draw3D` を呼ぶ。

`beginFrame` の中身:

1. コマンドアロケーターとコマンドリストをリセットする。 前のフレームは `waitForGpu` で終わっているので使い回せる。
2. バックバッファを `PRESENT` → `RENDER_TARGET` に移す (リソースバリア)。
3. 描画先 (色と深度) をつなぎ、 深度を 1.0 (いちばん奥) でクリアする。
4. ビューポートとシザーを画面全体にする。
5. カメラの行列を作り、 `DrawContext` に詰める。
6. 背景を描く。

`endFrame` の中身: `RENDER_TARGET` → `PRESENT` に戻す → 記録を閉じる → GPU へ送る → `Present` → GPU が終えるまで待つ。

> 毎フレーム GPU を待つのは遅いが、 いちばん簡単で間違えにくい。 氷の部品の説明が目的なので、 ここは簡単さを優先している。

## 氷の部品とのつなぎ目: DrawContext

```cpp
struct DrawContext {
    ID3D12GraphicsCommandList* commandList = nullptr;
    Float4x4 viewProj;
    Float3 cameraPos;
    float time = 0.0f;
};
```

| メンバー | 中身 |
| --- | --- |
| `commandList` | 記録中のコマンドリスト。 描画先・ビューポート・シザーは設定済みなので、 部品は自分のパイプラインを設定して描くだけ |
| `viewProj` | ワールド → クリップ空間の行列。 シェーダー用に **転置済み** |
| `cameraPos` | カメラの位置。 氷のシェーダーが光線を作るのに使う |
| `time` | 起動からの秒 |

自分のエンジンへ移すときは、 ここをエンジンの同じ役目の物に置き換える。

## 行列の約束 (MathTypes.h)

DirectXMath などの外部の数学ライブラリは使わず、 必要な分だけ自分で書いた。

| 約束 | 内容 |
| --- | --- |
| 行ベクトル | 点 `p` を行列 `M` で変えるときは `p × M`。 ワールド → クリップは `view × proj` |
| 行優先 | `m[行][列]` |
| 右手系 | y が上、 カメラは -z を向く |
| 奥行き | 射影後は 0 (手前) 〜 1 (奥) |

カメラの行列を作る所:

```cpp
const Float4x4 viewProj = camera.getViewMatrix() * camera.getProjectionMatrix(width / height);

DrawContext context;
context.viewProj = transpose(viewProj);
```

**なぜ転置するか**: HLSL の定数の行列は、 何も指定しないと **列優先** でメモリから読まれる。
行優先で並べた `M` をそのまま送ると、 シェーダーからは `M` の転置に見える。
先に転置して送ると、 シェーダーからは `M` そのものに見えるので、 CPU と同じ順で書ける。

```hlsl
float4 clip = mul(float4(worldPos, 1.0), ViewProj);  // CPU の p × M と同じ
```

`lookAtRH` (ワールド → ビュー):

```cpp
const Float3 zAxis = normalize(eye - target);      // 後ろ (カメラは -z を向くので)
const Float3 xAxis = normalize(cross(up, zAxis));  // 右
const Float3 yAxis = cross(zAxis, xAxis);          // 上
```

カメラの 3 本の軸を列に並べ、 最後の行でカメラの位置を引く。 こうすると、 カメラの位置が原点に、 注視点が -z の軸の上に来る。

`perspectiveFovRH` (ビュー → クリップ):

```cpp
const float range = farZ / (nearZ - farZ);
r.m[2][2] = range;
r.m[2][3] = -1.0f;  // w = -z (カメラの前にある点ほど z は負なので、 w は正になる)
r.m[3][2] = range * nearZ;
```

ビュー空間で `z = -nearZ` の点は奥行き 0、 `z = -farZ` の点は奥行き 1 になる (`z / w` で確かめられる)。

## 背景のシェーダー (Background.hlsl)

### 全画面の三角形

```hlsl
float2 uv = float2((id << 1) & 2, id & 2);
output.ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
output.position = float4(output.ndc, 0.0, 1.0);
```

| id | uv | 位置 |
| --- | --- | --- |
| 0 | (0, 0) | (-1, 1) 左上 |
| 1 | (2, 0) | (3, 1) 右の外 |
| 2 | (0, 2) | (-1, -3) 下の外 |

頂点バッファ無しで、 画面 (-1〜1 の四角) をすっぽり覆う三角形になる。 はみ出た所は切り捨てられる。

### 画素ごとの光線

```hlsl
float4 farPoint = mul(float4(ndc, 1.0, 1.0), InvViewProj);
float3 rd = normalize(farPoint.xyz / farPoint.w - CameraPos);
return ToDisplay(Environment(CameraPos, rd));
```

1. 画素を奥の面 (クリップ空間の z = 1) まで延ばした点を、 ビュー × 射影の **逆行列** でワールドへ戻す。
2. `w` で割って実際の位置にする (透視投影で掛かった `w` を外す)。
3. カメラからその点への向きが、 その画素を通る光線。
4. 光線の先の色 (`Environment`) をトーンマップとガンマで画面の色にする。

`Environment` は `Environment.hlsli` にあり、 **氷のシェーダーも同じ関数を使う**。 屈折した光線の先に見える景色と、 周りに描いた背景が同じ式なので、 氷越しの景色が周りとつながる。

## 確かめ方

| 項目 | どうなればよいか |
| --- | --- |
| 起動 | 空のグラデーションと、 地平線より下の灰色が出る |
| カメラ | 上下にドラッグすると地平線が動く。 Space で自動回転が止まる |
| 大きさ | ウィンドウの大きさを変えても伸びたり崩れたりしない |
| 読み直し | `Environment.hlsli` の色を変えて R を押すと変わる。 書き間違えるとエラーが出て、 前の絵のまま動き続ける |
