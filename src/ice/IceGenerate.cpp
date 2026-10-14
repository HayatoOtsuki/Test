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

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr float kPi = 3.14159265358979f;

constexpr float kCubeHalfSize = 0.4f;   // 立方体の半分の大きさ (1 辺 80 cm)
constexpr float kSphereRadius = 0.42f;  // 球の半径
constexpr int kSphereSlices = 48;       // 球の経線の数 (横の分け方)
constexpr int kSphereStacks = 24;       // 球の緯線の数 (縦の分け方)
constexpr float kCylinderRadius = 0.35f;  // 円柱の半径
constexpr float kCylinderHeight = 0.8f;   // 円柱の高さ
constexpr int kCylinderSlices = 48;       // 円柱の周りの分け方

// ---- SDF の氷 ----
constexpr int kSdfGridSize = 64;           // 格子の点の数 (1 辺)。 64³ = 262,144 点、 1 MB (float)
constexpr float kIceHalfSize = 0.38f;      // 氷の塊の半分の大きさ (角を丸める前の箱)
constexpr float kIceCornerRadius = 0.08f;  // 角の丸み
constexpr float kIceRipple = 0.006f;       // 表面のゆらぎの大きさ (6 mm)
constexpr float kIceChipRadius = 0.20f;    // 角の欠けを作る球の半径
constexpr float kSdfMargin = 0.06f;        // 格子を氷より少し広く取る余白 (表面の外にも距離が要るため)

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

// 中心 center、 半分の大きさ half の直方体の頂点とインデックスを足す (立方体と SDF の箱で使う)。
// 面ごとに頂点を 4 つ作る。 各面は 法線 n・面の右 u・面の上 v で決め、 u × v = n になるように選んである
// (この順に並べると、 外から見て反時計回りになる)
void appendBox(std::vector<IceVertex>& vertices, std::vector<uint32_t>& indices, Float3 center, Float3 half)
{
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
    // 向き a の成分ごとに、 半分の大きさを掛ける (面の中心までの距離・面の幅は向きごとに違う)
    auto scale = [&](Float3 a) { return Float3{ a.x * half.x, a.y * half.y, a.z * half.z }; };

    for (const Face& f : faces) {
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        const Float3 c = center + scale(f.n);  // 面の中心
        const Float3 u = scale(f.u);
        const Float3 v = scale(f.v);
        //   3 ---- 2      (0, 0) ---- (1, 0)   ← UV
        //   |      |        |           |
        //   0 ---- 1      (0, 1) ---- (1, 1)
        vertices.push_back({ c - u - v, f.n, { 0.0f, 1.0f } });
        vertices.push_back({ c + u - v, f.n, { 1.0f, 1.0f } });
        vertices.push_back({ c + u + v, f.n, { 1.0f, 0.0f } });
        vertices.push_back({ c - u + v, f.n, { 0.0f, 0.0f } });
        // 四角形を三角形 2 枚に: 0-1-2 と 0-2-3 (どちらも反時計回り)
        indices.insert(indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    }
}

// ---- SDF の部品 (どれも 「点 p から形の表面までの符号付きの距離」 を返す。 外が正、 中が負) ----

// 角を丸めた箱。 half: 丸める前の半分の大きさ、 r: 角の丸みの半径
float sdRoundBox(Float3 p, Float3 half, float r)
{
    // 1. 箱を r だけ小さくした箱 (half - r) を考え、 p をその表面からの外への出っ張り q に直す
    //    (abs で 8 つの象限を 1 つにまとめる。 箱は対称なので +x+y+z の 1/8 だけ考えればよい)
    const Float3 q = { std::fabs(p.x) - (half.x - r), std::fabs(p.y) - (half.y - r), std::fabs(p.z) - (half.z - r) };
    // 2. 外: 出っ張っている成分だけの長さ。 中: いちばん近い面までの距離 (負)
    const Float3 outside = { std::max(q.x, 0.0f), std::max(q.y, 0.0f), std::max(q.z, 0.0f) };
    const float inside = std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
    // 3. 小さくした箱から r だけ外へふくらませると、 角が半径 r で丸くなる
    return std::sqrt(dot(outside, outside)) + inside - r;
}

// 球。 c: 中心、 r: 半径
float sdSphere(Float3 p, Float3 c, float r)
{
    const Float3 d = p - c;
    return std::sqrt(dot(d, d)) - r;
}

// なめらかな最大 (max の角を、 幅 k の 2 次式で丸めたもの)。
// a と b が k 以上離れていれば max(a, b) と同じ。 近いときは少し大きくなり、 2 つの形の境目がなめらかにつながる
float smoothMax(float a, float b, float k)
{
    const float h = std::max(k - std::fabs(a - b), 0.0f) / k;  // 0 (離れている) 〜 1 (同じ値)
    return std::max(a, b) + h * h * k * 0.25f;
}

// 整数の格子点 (x, y, z) から、 0〜1 の決まった乱数を作る (ハッシュ)
float hash3(int x, int y, int z)
{
    uint32_t h = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u ^
                 static_cast<uint32_t>(z) * 83492791u;
    h = (h ^ (h >> 13)) * 1274126177u;  // ビットをよく混ぜる
    h ^= h >> 16;
    return static_cast<float>(h & 0xffffffu) / static_cast<float>(0xffffff);
}

// 値ノイズ: 格子点に乱数を置き、 その間をなめらかに補間した、 ゆっくり変わる -1〜1 の値
float valueNoise(Float3 p)
{
    const int ix = static_cast<int>(std::floor(p.x)), iy = static_cast<int>(std::floor(p.y)),
              iz = static_cast<int>(std::floor(p.z));
    const float fx = p.x - std::floor(p.x), fy = p.y - std::floor(p.y), fz = p.z - std::floor(p.z);
    // 3 次のなめらかな曲線 (smoothstep) で補間の重みを作る。 直線で補間すると格子の境目に折れ目が出る
    const float ux = fx * fx * (3.0f - 2.0f * fx), uy = fy * fy * (3.0f - 2.0f * fy), uz = fz * fz * (3.0f - 2.0f * fz);
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    // 立方体の 8 つの角の乱数を、 x → y → z の順に補間する (3 重の線形補間)
    float c[2][2];
    for (int j = 0; j < 2; ++j)
        for (int k = 0; k < 2; ++k) c[j][k] = lerp(hash3(ix, iy + j, iz + k), hash3(ix + 1, iy + j, iz + k), ux);
    const float v = lerp(lerp(c[0][0], c[1][0], uy), lerp(c[0][1], c[1][1], uy), uz);
    return v * 2.0f - 1.0f;
}

// 氷の塊の SDF (物体の空間。 原点は底の真ん中)
float iceShapeSdf(Float3 p)
{
    // 1. 角を丸めた箱。 底が y = 0 に載るよう、 中心を半分の大きさだけ上げる
    const Float3 center = { 0.0f, kIceHalfSize, 0.0f };
    float d = sdRoundBox(p - center, { kIceHalfSize, kIceHalfSize, kIceHalfSize }, kIceCornerRadius);

    // 2. 角の欠け: 上の角の 1 つに球を置き、 その分を削る。
    //    削る = 「箱の中」 かつ 「球の外」。 SDF では max(箱, -球)。 なめらかな最大で、 削った縁を丸める
    const Float3 chipCenter = { kIceHalfSize, 2.0f * kIceHalfSize, kIceHalfSize };
    d = smoothMax(d, -sdSphere(p, chipCenter, kIceChipRadius), 0.05f);

    // 3. 表面のゆらぎ: 大きさの違う 2 つのノイズを重ねて、 距離を少しずらす (溶けかけた表面の凸凹)。
    //    ずらすと厳密な距離ではなくなるが、 ゆらぎが小さいので球追跡には問題ない
    const float n = valueNoise(p * 9.0f) * 0.7f + valueNoise(p * 23.0f) * 0.3f;
    return d + n * kIceRipple;
}

} // namespace

// -----------------------------------------------------------------------------
// 全部の形を作る
// -----------------------------------------------------------------------------

void IceImplement::generateShape(ID3D12GraphicsCommandList* commandList)
{
    // 形の種類を 0 から順に回し、 種類ごとの作り方を switch で選ぶ。
    // 作った形は、 種類と同じ番号の mGeom に入れる (描くときは mGeom[今の形] を選ぶだけ)
    for (int i = 0; i < static_cast<int>(IceShape::Count); ++i) {
        switch (static_cast<IceShape>(i)) {
        case IceShape::Cube: mGeom[i] = generateCube(); break;
        case IceShape::Sphere: mGeom[i] = generateSphere(); break;
        case IceShape::Cylinder: mGeom[i] = generateCylinder(); break;
        case IceShape::Sdf: mGeom[i] = generateSDF(commandList); break;
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
    // 共有すると、 角で法線と UV が 1 つに決まらないため。 作り方は appendBox を参照
    const float h = kCubeHalfSize;
    std::vector<IceVertex> vertices;
    std::vector<uint32_t> indices;
    // 底 (y = 0) が床に載るよう、 中心を半分だけ上げる
    appendBox(vertices, indices, { 0.0f, h, 0.0f }, { h, h, h });
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
// 円柱
// -----------------------------------------------------------------------------

IceGeometry IceImplement::generateCylinder()
{
    // 側面と、 上と下のふた (円) の 3 つに分けて作る。
    // 側面とふたの境目 (縁) では法線が急に変わるので、 縁の頂点は側面用とふた用で別に持つ。
    const float r = kCylinderRadius;
    const float h = kCylinderHeight;
    std::vector<IceVertex> vertices;
    std::vector<uint32_t> indices;

    // ---- 側面: 上の輪 (y = h) と下の輪 (y = 0) を、 球と同じ並べ方の四角形でつなぐ ----
    // theta = 0 で +z、 theta が増えると +x の側へ回る (外から見て右へ進む)
    for (int i = 0; i <= 1; ++i) {
        const float y = (i == 0) ? h : 0.0f;  // i = 0 が上の輪、 i = 1 が下の輪
        for (int j = 0; j <= kCylinderSlices; ++j) {
            const float theta = 2.0f * kPi * static_cast<float>(j) / kCylinderSlices;
            const Float3 n = { std::sin(theta), 0.0f, std::cos(theta) };  // 側面の法線は真横 (外向き)
            const Float2 uv = { static_cast<float>(j) / kCylinderSlices, static_cast<float>(i) };
            vertices.push_back({ { n.x * r, y, n.z * r }, n, uv });
        }
    }
    const uint32_t row = kCylinderSlices + 1;
    for (uint32_t j = 0; j < kCylinderSlices; ++j) {
        // a-b が上の輪、 c-d が下の輪 (球の generateSphere と同じ図)
        const uint32_t a = j, b = j + 1, c = j + row, d = c + 1;
        indices.insert(indices.end(), { c, d, b, c, b, a });
    }

    // ---- ふた: 真ん中の頂点 1 つと縁の輪を、 扇のように三角形でつなぐ ----
    // 上のふたは法線 +y。 上から見て反時計回りにするには、 真ん中 → 縁の j → 縁の j + 1 の順
    // (theta が増える向きは、 上から見ると反時計回り)。 下のふたは裏から見ることになるので逆の順にする
    for (int cap = 0; cap < 2; ++cap) {
        const bool top = (cap == 0);
        const float y = top ? h : 0.0f;
        const Float3 n = { 0.0f, top ? 1.0f : -1.0f, 0.0f };
        const uint32_t center = static_cast<uint32_t>(vertices.size());
        vertices.push_back({ { 0.0f, y, 0.0f }, n, { 0.5f, 0.5f } });
        for (int j = 0; j <= kCylinderSlices; ++j) {
            const float theta = 2.0f * kPi * static_cast<float>(j) / kCylinderSlices;
            const float s = std::sin(theta), c = std::cos(theta);
            // UV は円をそのまま 0〜1 の四角に写す
            vertices.push_back({ { s * r, y, c * r }, n, { 0.5f + 0.5f * s, 0.5f - 0.5f * c } });
        }
        for (uint32_t j = 0; j < kCylinderSlices; ++j) {
            const uint32_t p0 = center + 1 + j, p1 = p0 + 1;
            if (top) {
                indices.insert(indices.end(), { center, p0, p1 });
            } else {
                indices.insert(indices.end(), { center, p1, p0 });
            }
        }
    }
    return createGeometry(vertices, indices);
}

// -----------------------------------------------------------------------------
// SDF の氷
// -----------------------------------------------------------------------------

IceGeometry IceImplement::generateSDF(ID3D12GraphicsCommandList* commandList)
{
    // ---- 1. 格子が覆う箱 (物体の空間) を決める ----
    // 氷の塊 (底の真ん中が原点、 高さ 2 × kIceHalfSize) に、 余白 kSdfMargin を足した立方体
    const float half = kIceHalfSize + kSdfMargin;
    const Float3 boundsMin = { -half, kIceHalfSize - half, -half };
    const Float3 boundsMax = { half, kIceHalfSize + half, half };
    const int n = kSdfGridSize;
    const float voxel = (2.0f * half) / static_cast<float>(n);  // 格子 1 つ分の大きさ

    // ---- 2. CPU で SDF を焼く ----
    // 格子の点 (x, y, z) は、 箱を n 等分したマス目の 「真ん中」 に置く。
    // テクスチャを線形補間で読むとき、 テクセルの中心が uvw = (i + 0.5) / n に当たるので、 そこに合わせる
    std::vector<float> grid(static_cast<size_t>(n) * n * n);
    for (int z = 0; z < n; ++z) {
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                const Float3 p = { boundsMin.x + (static_cast<float>(x) + 0.5f) * voxel,
                                   boundsMin.y + (static_cast<float>(y) + 0.5f) * voxel,
                                   boundsMin.z + (static_cast<float>(z) + 0.5f) * voxel };
                // 並びは x が最も速く変わる (行) → y (行の並び) → z (面の並び)。 テクスチャのメモリの並びと同じ
                grid[(static_cast<size_t>(z) * n + y) * n + x] = iceShapeSdf(p);
            }
        }
    }

    IceGeometry geom;
    geom.isSdf = true;
    geom.boundsMin = boundsMin;
    geom.boundsMax = boundsMax;
    geom.voxelSize = voxel;

    // ---- 3. 3D テクスチャを作る (GPU 専用のメモリ = 既定ヒープ) ----
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    texDesc.Width = n;
    texDesc.Height = n;
    texDesc.DepthOrArraySize = static_cast<UINT16>(n);  // 3D テクスチャでは奥行き
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R32_FLOAT;  // 距離を 32 bit の浮動小数で 1 つ
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;  // メモリの並びは GPU に任せる

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    // 最初は 「写される先 (COPY_DEST)」 の状態で作る
    ThrowIfFailed(mDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
                                                   D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                   IID_PPV_ARGS(&geom.sdfTexture)),
                  "CreateCommittedResource(SDF texture)");

    // ---- 4. 送るための一時バッファ (アップロードヒープ) に、 GPU の決まりの並びで書く ----
    // テクスチャへ写すとき、 バッファ側の 1 行の大きさ (RowPitch) は 256 バイトの倍数でなければならない。
    // GetCopyableFootprints が、 この決まりに合う並び (行の大きさ・全体の大きさ) を教えてくれる
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT rowCount = 0;
    UINT64 rowBytes = 0, totalBytes = 0;
    mDevice->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &rowCount, &rowBytes, &totalBytes);

    std::vector<uint8_t> staging(static_cast<size_t>(totalBytes));
    const UINT rowPitch = footprint.Footprint.RowPitch;
    const UINT64 slicePitch = static_cast<UINT64>(rowPitch) * rowCount;  // 1 枚 (z が同じ面) の大きさ
    for (int z = 0; z < n; ++z) {
        for (UINT y = 0; y < rowCount; ++y) {
            // 1 行ずつ写す (行の後ろには、 256 バイトの倍数にそろえるための隙間がありうる)
            uint8_t* dst = staging.data() + footprint.Offset + z * slicePitch + static_cast<UINT64>(y) * rowPitch;
            const float* src = grid.data() + (static_cast<size_t>(z) * n + y) * n;
            std::memcpy(dst, src, static_cast<size_t>(rowBytes));
        }
    }
    geom.sdfUpload = createUploadBuffer(mDevice, staging.data(), staging.size());

    // ---- 5. 写す命令と、 状態を変える命令をコマンドリストに記録する ----
    // ここでは記録するだけ。 GPU で実行されるのは、 このフレームのコマンドリストが送られたとき (描画より前)。
    // それまで一時バッファを壊してはいけないので、 geom.sdfUpload に持っておく
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = geom.sdfTexture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = geom.sdfUpload.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;
    commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    // 写し終えたら 「ピクセルシェーダーから読む (PIXEL_SHADER_RESOURCE)」 状態へ
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = geom.sdfTexture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);

    // ---- 6. SRV (シェーダーから読むための記述子) をヒープに作る ----
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = DXGI_FORMAT_R32_FLOAT;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;  // r g b a をそのまま
    srv.Texture3D.MipLevels = 1;
    mDevice->CreateShaderResourceView(geom.sdfTexture.Get(), &srv, mSrvHeap->GetCPUDescriptorHandleForHeapStart());

    // ---- 7. 格子が覆う箱のメッシュ ----
    // この箱を描くと、 箱が画面に映る画素でだけピクセルシェーダーが動き、 そこで光線を追う
    std::vector<IceVertex> vertices;
    std::vector<uint32_t> indices;
    const Float3 center = { 0.0f, kIceHalfSize, 0.0f };
    appendBox(vertices, indices, center, { half, half, half });
    IceGeometry box = createGeometry(vertices, indices);
    geom.vertexBuffer = box.vertexBuffer;
    geom.indexBuffer = box.indexBuffer;
    geom.vertexView = box.vertexView;
    geom.indexView = box.indexView;
    geom.indexCount = box.indexCount;
    return geom;
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
