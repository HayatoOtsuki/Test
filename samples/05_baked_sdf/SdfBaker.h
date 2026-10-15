#pragma once
// =============================================================================
// SdfBaker.h — SDF を格子に焼く (CPU だけ。 GPU は使わない)
//
// 立方体の箱を n × n × n のマスに分け、 各マスの真ん中で SDF の式を計算して、 1 列の配列に並べる。
// 並びは x が最も速く変わる (行) → y (行の並び) → z (面の並び)。 3D テクスチャのメモリの並びと同じなので、
// そのまま GPU へ写せる (SdfTexture)。
// =============================================================================

#include "engine/MathTypes.h"

#include <vector>

// 焼いた結果
struct SdfGrid {
    int size = 0;               // 1 辺のマスの数 n (全部で n³)
    Float3 boundsMin;           // 格子が覆う箱
    Float3 boundsMax;
    float voxelSize = 0.0f;     // 1 マスの大きさ (メートル)
    std::vector<float> values;  // 距離。 マス (x, y, z) の値は values[(z * n + y) * n + x]
};

// 点を受け取って距離を返す関数 (BakedShape の sdf など)
using SdfFunction = float (*)(const Float3& p);

// 箱 [boundsMin, boundsMax] (立方体であること) を size × size × size のマスに分けて焼く
SdfGrid bakeSdf(int size, const Float3& boundsMin, const Float3& boundsMax, SdfFunction sdf);
