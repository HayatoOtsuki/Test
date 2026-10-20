#pragma once
// =============================================================================
// EnvironmentImage.h — 環境マップ (キューブマップ) の 6 面を CPU で作る
//
// 氷のシェーダーは、 周りの景色を環境マップから読む (向きを渡すと、 その向きの景色の色が返る)。
// ここでは背景と同じ式 (エンジンの代わりの Environment.hlsli: 空・太陽・床・霧) で、 6 面の色を計算する。
// 自分のエンジンでは、 スカイボックスのキューブマップを、 そのまま環境マップとして渡せばよい。
//
// 面の並びと向きは D3D のキューブマップの決まり (+X, −X, +Y, −Y, +Z, −Z)。
// 各面の画素 (x, y) は、 s = (x + 0.5) / n、 t = (y + 0.5) / n (t は上から下)、 u = 2s − 1、 v = 2t − 1 として、
//   +X: ( 1, −v, −u)   −X: (−1, −v,  u)   +Y: ( u,  1,  v)
//   −Y: ( u, −1, −v)   +Z: ( u, −v,  1)   −Z: (−u, −v, −1)
// の向きの景色の色になる (CPU だけ。 GPU は使わない)。
// =============================================================================

#include "engine/MathTypes.h"

#include <vector>

namespace environment_image {

// 環境マップを見る点 (ワールド)。 コップの真ん中 (氷のあたり)。 床の模様は、 ここから見た物になる
Float3 probePosition();

// 背景と同じ景色の色 (光の強さのまま)。 ro から rd の向きに見える色。 lightDirection: 太陽への向き、 lightColor: 太陽の色 × 強さ
Float3 environment(const Float3& ro, const Float3& rd, const Float3& lightDirection, const Float3& lightColor);

// キューブマップの面 face (0〜5) の画素 (x, y) が表す向き (長さ 1)。 size: 1 辺の画素の数
Float3 faceDirection(int face, int x, int y, int size);

// 6 面ぶんの色 (RGBA の float、 面 → 行 → 列 の順に並ぶ)。 α は 1
std::vector<float> makeFaces(int size, const Float3& lightDirection, const Float3& lightColor);

} // namespace environment_image
