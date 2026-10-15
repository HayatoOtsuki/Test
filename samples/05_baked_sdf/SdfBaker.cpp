#include "SdfBaker.h"

SdfGrid bakeSdf(int size, const Float3& boundsMin, const Float3& boundsMax, SdfFunction sdf)
{
    SdfGrid grid;
    grid.size = size;
    grid.boundsMin = boundsMin;
    grid.boundsMax = boundsMax;
    // 箱は立方体なので、 x の幅を n 等分すれば 1 マスの大きさになる
    grid.voxelSize = (boundsMax.x - boundsMin.x) / static_cast<float>(size);
    grid.values.resize(static_cast<size_t>(size) * size * size);

    for (int z = 0; z < size; ++z) {
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                // マスの 「真ん中」 で計算する。 GPU がテクスチャを線形補間で読むとき、
                // テクセル (マス) の中心が uvw = (i + 0.5) / n に当たるので、 そこに合わせる
                const Float3 p = { boundsMin.x + (static_cast<float>(x) + 0.5f) * grid.voxelSize,
                                   boundsMin.y + (static_cast<float>(y) + 0.5f) * grid.voxelSize,
                                   boundsMin.z + (static_cast<float>(z) + 0.5f) * grid.voxelSize };
                grid.values[(static_cast<size_t>(z) * size + y) * size + x] = sdf(p);
            }
        }
    }
    return grid;
}
