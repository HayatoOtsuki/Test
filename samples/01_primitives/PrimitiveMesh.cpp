// =============================================================================
// PrimitiveMesh.cpp — 立方体・球・円柱の頂点とインデックス
//
// 解説: samples/01_primitives/README.md (本体の docs/03-ice-component.md §3〜6 と同じ作り方)
// =============================================================================

#include "PrimitiveMesh.h"

#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979f;

} // namespace

namespace primitive {

MeshData makeCube(float half)
{
    // 面を 3 本の向きで決める: 外向きの法線 n、 面の右 u、 面の上 v。
    // u × v = n になるように選ぶと、 下の順 (0-1-2, 0-2-3) が外から見て反時計回りになる
    struct Face {
        Float3 n, u, v;
    };
    const Face faces[6] = {
        { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } },   // +x (右)
        { { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },   // -x (左)
        { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } },   // +y (上)
        { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },   // -y (下)
        { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } },    // +z (手前)
        { { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 } },  // -z (奥)
    };
    const Float3 center = { 0.0f, half, 0.0f };  // 底 (y = 0) が床に載るよう、 中心を半分だけ上げる

    MeshData mesh;
    for (const Face& f : faces) {
        const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        const Float3 c = center + f.n * half;  // 面の中心
        //   3 ---- 2      (0, 0) ---- (1, 0)   ← UV
        //   |      |        |           |
        //   0 ---- 1      (0, 1) ---- (1, 1)
        mesh.vertices.push_back({ c - f.u * half - f.v * half, f.n, { 0.0f, 1.0f } });
        mesh.vertices.push_back({ c + f.u * half - f.v * half, f.n, { 1.0f, 1.0f } });
        mesh.vertices.push_back({ c + f.u * half + f.v * half, f.n, { 1.0f, 0.0f } });
        mesh.vertices.push_back({ c - f.u * half + f.v * half, f.n, { 0.0f, 0.0f } });
        mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    }
    return mesh;
}

MeshData makeSphere(float radius, int slices, int stacks)
{
    // 地球儀のような格子。 phi: 真上 (0) から真下 (π)、 theta: ぐるりと 1 周 (0〜2π)。
    // 球の上の向き n(phi, theta) = (sin phi sin theta, cos phi, sin phi cos theta) が、 そのまま外向きの法線
    const Float3 center = { 0.0f, radius, 0.0f };
    MeshData mesh;
    for (int i = 0; i <= stacks; ++i) {
        const float phi = kPi * static_cast<float>(i) / static_cast<float>(stacks);
        for (int j = 0; j <= slices; ++j) {
            const float theta = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(slices);
            const Float3 n = { std::sin(phi) * std::sin(theta), std::cos(phi), std::sin(phi) * std::cos(theta) };
            const Float2 uv = { static_cast<float>(j) / static_cast<float>(slices),
                                static_cast<float>(i) / static_cast<float>(stacks) };
            mesh.vertices.push_back({ center + n * radius, n, uv });
        }
    }
    // 格子の四角形 1 つを三角形 2 枚に (a-b が上の段、 c-d が下の段。 theta が増える向きは外から見て右)
    const uint32_t row = static_cast<uint32_t>(slices) + 1;
    for (uint32_t i = 0; i < static_cast<uint32_t>(stacks); ++i) {
        for (uint32_t j = 0; j < static_cast<uint32_t>(slices); ++j) {
            const uint32_t a = i * row + j, b = a + 1, c = a + row, d = c + 1;
            mesh.indices.insert(mesh.indices.end(), { c, d, b, c, b, a });
        }
    }
    return mesh;
}

MeshData makeCylinder(float radius, float height, int slices)
{
    MeshData mesh;
    // ---- 側面: 上の輪と下の輪を、 球と同じ並べ方でつなぐ。 法線は真横 ----
    for (int i = 0; i <= 1; ++i) {
        const float y = (i == 0) ? height : 0.0f;
        for (int j = 0; j <= slices; ++j) {
            const float theta = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(slices);
            const Float3 n = { std::sin(theta), 0.0f, std::cos(theta) };
            mesh.vertices.push_back({ { n.x * radius, y, n.z * radius }, n,
                                      { static_cast<float>(j) / static_cast<float>(slices), static_cast<float>(i) } });
        }
    }
    const uint32_t row = static_cast<uint32_t>(slices) + 1;
    for (uint32_t j = 0; j < static_cast<uint32_t>(slices); ++j) {
        const uint32_t a = j, b = j + 1, c = j + row, d = c + 1;
        mesh.indices.insert(mesh.indices.end(), { c, d, b, c, b, a });
    }
    // ---- ふた: 真ん中の頂点と縁の輪を扇のようにつなぐ。 縁の頂点は側面と別にする (法線が違うため) ----
    for (int cap = 0; cap < 2; ++cap) {
        const bool top = (cap == 0);
        const float y = top ? height : 0.0f;
        const Float3 n = { 0.0f, top ? 1.0f : -1.0f, 0.0f };
        const uint32_t center = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({ { 0.0f, y, 0.0f }, n, { 0.5f, 0.5f } });
        for (int j = 0; j <= slices; ++j) {
            const float theta = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(slices);
            const float s = std::sin(theta), c = std::cos(theta);
            mesh.vertices.push_back({ { s * radius, y, c * radius }, n, { 0.5f + 0.5f * s, 0.5f - 0.5f * c } });
        }
        for (uint32_t j = 0; j < static_cast<uint32_t>(slices); ++j) {
            const uint32_t p0 = center + 1 + j, p1 = p0 + 1;
            // 上のふた: 真ん中 → j → j + 1 が上から見て反時計回り。 下のふたは裏から見るので逆の順
            if (top) mesh.indices.insert(mesh.indices.end(), { center, p0, p1 });
            else mesh.indices.insert(mesh.indices.end(), { center, p1, p0 });
        }
    }
    return mesh;
}

} // namespace primitive
