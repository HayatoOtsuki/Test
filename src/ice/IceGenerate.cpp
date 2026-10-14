// =============================================================================
// IceGenerate.cpp — IceImplement の形づくり
//
// generateShape が for と switch で全部の形を作り、 mGeom に入れる。
// 形ごとの作り方は generateXxx に分けてある。 形を足すときは:
//   1. IceShape (IceComponent.h) に種類を足す (Count の前)
//   2. generateXxx を書く
//   3. generateShape の switch に case を足す
//
// 形の約束:
//   ・物体の空間で、 原点は底の真ん中。 y = 0 に置くと床に載る
//   ・三角形は外から見て反時計回り (右手で回すと親指が外を向く順)。 パイプラインはこれを表とし、 裏はカリングする
//   ・UV は左上が (0, 0)、 右下が (1, 1)
// =============================================================================

#include "IceImplement.h"

#include "engine/DxCheck.h"

#include <cmath>
#include <cstring>

namespace {

constexpr float kPi = 3.14159265358979f;

constexpr float kCubeHalfSize = 0.4f;   // 立方体の半分の大きさ (1 辺 80 cm)
constexpr float kSphereRadius = 0.42f;  // 球の半径
constexpr int kSphereSlices = 48;       // 球の経線の数 (横の分け方)
constexpr int kSphereStacks = 24;       // 球の緯線の数 (縦の分け方)

// CPU から書けて GPU から読めるバッファ (アップロードヒープ) を作り、 data を写す。
// 頂点とインデックスは作った後に変えず、 小さいので、 GPU 専用のメモリへ移さずにここに置いたまま使う
Microsoft::WRL::ComPtr<ID3D12Resource> createUploadBuffer(ID3D12Device* device, const void* data, size_t size)
{
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;  // バッファは必ずこの並び

    // アップロードヒープのリソースは GENERIC_READ の状態で作る決まり
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                  nullptr, IID_PPV_ARGS(&buffer)),
                  "CreateCommittedResource(upload buffer)");

    // Map で CPU から見えるアドレスを受け取り、 写して、 Unmap で戻す。
    // 読む範囲 {0, 0} は 「CPU からは読まない」 という意味
    void* mapped = nullptr;
    const D3D12_RANGE noRead = { 0, 0 };
    ThrowIfFailed(buffer->Map(0, &noRead, &mapped), "Map(upload buffer)");
    std::memcpy(mapped, data, size);
    buffer->Unmap(0, nullptr);
    return buffer;
}

} // namespace

// -----------------------------------------------------------------------------
// 全部の形を作る
// -----------------------------------------------------------------------------

void IceImplement::generateShape()
{
    // 形の種類を 0 から順に回し、 種類ごとの作り方を switch で選ぶ。
    // 作った形は、 種類と同じ番号の mGeom に入れる (描くときは mGeom[今の形] を選ぶだけ)
    for (int i = 0; i < static_cast<int>(IceShape::Count); ++i) {
        switch (static_cast<IceShape>(i)) {
        case IceShape::Cube: mGeom[i] = generateCube(); break;
        case IceShape::Sphere: mGeom[i] = generateSphere(); break;
        default: break;
        }
    }
}

// -----------------------------------------------------------------------------
// 立方体
// -----------------------------------------------------------------------------

IceGeometry IceImplement::generateCube()
{
    // 面ごとに頂点を 4 つ作る (角の頂点を面どうしで共有しない)。
    // 共有すると、 角で法線と UV が 1 つに決まらないため。
    //
    // 面は 3 本の向きで決める: 法線 n (外向き)、 面の右 u、 面の上 v。
    // u × v = n になるように選んであるので、 下の順に並べると外から見て反時計回りになる。
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

    const float h = kCubeHalfSize;
    const Float3 center = { 0.0f, h, 0.0f };  // 底 (y = 0) が床に載るよう、 中心を半分だけ上げる

    std::vector<IceVertex> vertices;
    std::vector<uint32_t> indices;
    for (const Face& f : faces) {
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        const Float3 c = center + f.n * h;  // 面の中心
        //   3 ---- 2      (0, 0) ---- (1, 0)   ← UV
        //   |      |        |           |
        //   0 ---- 1      (0, 1) ---- (1, 1)
        vertices.push_back({ c - f.u * h - f.v * h, f.n, { 0.0f, 1.0f } });
        vertices.push_back({ c + f.u * h - f.v * h, f.n, { 1.0f, 1.0f } });
        vertices.push_back({ c + f.u * h + f.v * h, f.n, { 1.0f, 0.0f } });
        vertices.push_back({ c - f.u * h + f.v * h, f.n, { 0.0f, 0.0f } });
        // 四角形を三角形 2 枚に: 0-1-2 と 0-2-3 (どちらも反時計回り)
        indices.insert(indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    }
    return createGeometry(vertices, indices);
}

// -----------------------------------------------------------------------------
// 球
// -----------------------------------------------------------------------------

IceGeometry IceImplement::generateSphere()
{
    // 地球儀のように、 緯度 (縦) と経度 (横) の格子で作る。
    //   phi   : 真上 (0) から真下 (π) までの角度。 kSphereStacks 等分
    //   theta : ぐるりと 1 周 (0〜2π)。 kSphereSlices 等分
    // 格子の点の数は (stacks + 1) × (slices + 1)。 経度 0 と 2π の点は同じ位置だが、
    // UV の u が 0 と 1 で違うので別の頂点にする (継ぎ目で模様が切れないように)。
    const float r = kSphereRadius;
    const Float3 center = { 0.0f, r, 0.0f };  // 底が床に載るよう、 半径だけ上げる

    std::vector<IceVertex> vertices;
    for (int i = 0; i <= kSphereStacks; ++i) {
        const float phi = kPi * static_cast<float>(i) / kSphereStacks;
        for (int j = 0; j <= kSphereSlices; ++j) {
            const float theta = 2.0f * kPi * static_cast<float>(j) / kSphereSlices;
            // 球の上の向き (長さ 1)。 これがそのまま外向きの法線になる。
            // theta = 0 で +z、 theta が増えると +x の側へ回る
            const Float3 n = { std::sin(phi) * std::sin(theta), std::cos(phi), std::sin(phi) * std::cos(theta) };
            const Float2 uv = { static_cast<float>(j) / kSphereSlices, static_cast<float>(i) / kSphereStacks };
            vertices.push_back({ center + n * r, n, uv });
        }
    }

    // 格子の四角形 1 つを三角形 2 枚に。 外から見ると:
    //   a ---- b     a = (i, j)     b = (i, j + 1)     ← 上 (phi が小さい)
    //   |      |
    //   c ---- d     c = (i + 1, j) d = (i + 1, j + 1) ← 下
    // theta が増える向き (a → b) は、 外から見て右。 c-d-b と c-b-a が反時計回り。
    // 極 (i = 0 と i = stacks) では a と b が同じ位置になり、 片方の三角形は面積 0 になるが、 描いても害は無い
    std::vector<uint32_t> indices;
    const uint32_t row = kSphereSlices + 1;  // 1 段の頂点の数
    for (int i = 0; i < kSphereStacks; ++i) {
        for (int j = 0; j < kSphereSlices; ++j) {
            const uint32_t a = static_cast<uint32_t>(i) * row + static_cast<uint32_t>(j);
            const uint32_t b = a + 1;
            const uint32_t c = a + row;
            const uint32_t d = c + 1;
            indices.insert(indices.end(), { c, d, b, c, b, a });
        }
    }
    return createGeometry(vertices, indices);
}

// -----------------------------------------------------------------------------
// GPU のバッファにする
// -----------------------------------------------------------------------------

IceGeometry IceImplement::createGeometry(const std::vector<IceVertex>& vertices, const std::vector<uint32_t>& indices)
{
    IceGeometry geom;
    const size_t vertexBytes = vertices.size() * sizeof(IceVertex);
    const size_t indexBytes = indices.size() * sizeof(uint32_t);

    geom.vertexBuffer = createUploadBuffer(mDevice, vertices.data(), vertexBytes);
    geom.indexBuffer = createUploadBuffer(mDevice, indices.data(), indexBytes);

    // ビュー: GPU のアドレス・全体の大きさ・1 つ分の大きさ (頂点) か形式 (インデックス)
    geom.vertexView.BufferLocation = geom.vertexBuffer->GetGPUVirtualAddress();
    geom.vertexView.SizeInBytes = static_cast<UINT>(vertexBytes);
    geom.vertexView.StrideInBytes = sizeof(IceVertex);

    geom.indexView.BufferLocation = geom.indexBuffer->GetGPUVirtualAddress();
    geom.indexView.SizeInBytes = static_cast<UINT>(indexBytes);
    geom.indexView.Format = DXGI_FORMAT_R32_UINT;  // インデックスは 32 bit (65536 個を超えても使えるように)

    geom.indexCount = static_cast<UINT>(indices.size());
    return geom;
}
