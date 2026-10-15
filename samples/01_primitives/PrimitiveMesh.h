#pragma once
// =============================================================================
// PrimitiveMesh.h — プリミティブの形 (頂点とインデックス) を作る
//
// GPU を使わない、 ただの計算。 だから手元でテストできる (三角形の向きと法線が合っているか)。
//
// 形の約束:
//   ・原点は底の真ん中 (y = 0 に置くと床に載る)
//   ・三角形は外から見て反時計回り (右手で回すと親指が外を向く順)
//   ・UV は左上が (0, 0)、 右下が (1, 1)
// =============================================================================

#include "engine/MathTypes.h"

#include <cstdint>
#include <vector>

// 頂点 1 つ (32 バイト)。 シェーダーの VSInput と同じ並び
struct MeshVertex {
    Float3 position;  // 位置 (メートル)
    Float3 normal;    // 外向きの法線 (長さ 1)
    Float2 uv;        // テクスチャ座標
};

// 形 1 つ分: 頂点の並びと、 三角形を作る頂点の番号の並び (3 つで 1 枚)
struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
};

namespace primitive {

// 立方体。 half: 半分の大きさ。 面ごとに頂点を 4 つ (角の頂点を面どうしで共有しない)
MeshData makeCube(float half);

// 球。 radius: 半径。 slices: 横 (経度) の分け方、 stacks: 縦 (緯度) の分け方
MeshData makeSphere(float radius, int slices, int stacks);

// 円柱。 radius: 半径、 height: 高さ、 slices: 周りの分け方。 側面と上下のふた
MeshData makeCylinder(float radius, float height, int slices);

} // namespace primitive
