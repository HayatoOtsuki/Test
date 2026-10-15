#include "BakedShape.h"

#include <algorithm>
#include <cmath>

namespace {

const Float3 kCenter = { 0.0f, 0.45f, 0.0f };  // 形の真ん中 (02 / 04 の形と同じ高さ)
constexpr float kBoundsHalf = 0.5f;            // 格子の箱の半分の大きさ
constexpr float kBoxHalf = 0.3f;               // 箱の半分の大きさ (1 辺 60 cm)
constexpr float kScoopRadius = 0.22f;          // 角をえぐる球の半径
constexpr float kRingRadius = 0.42f;           // 輪の半径 (管の真ん中の線まで)
constexpr float kRingThickness = 0.035f;       // 管の半径 (太さ 7 cm)

float length(const Float3& v)
{
    return std::sqrt(dot(v, v));
}

// 箱 (半分の大きさ b)。 式の意味は 02 の README と同じ (HLSL の sdBox を C++ にした物)
float sdBox(const Float3& p, const Float3& b)
{
    const Float3 q = { std::fabs(p.x) - b.x, std::fabs(p.y) - b.y, std::fabs(p.z) - b.z };
    const Float3 outside = { std::max(q.x, 0.0f), std::max(q.y, 0.0f), std::max(q.z, 0.0f) };
    return length(outside) + std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
}

// 球 (半径 r)
float sdSphere(const Float3& p, float r)
{
    return length(p) - r;
}

// トーラス (輪の半径 bigR、 管の半径 r)。 横 (xz) の距離で輪の真ん中の線までを測り、 縦 (y) と合わせて管までの距離にする
float sdTorus(const Float3& p, float bigR, float r)
{
    const float ringX = std::sqrt(p.x * p.x + p.z * p.z) - bigR;
    return std::sqrt(ringX * ringX + p.y * p.y) - r;
}

} // namespace

namespace baked_shape {

Float3 boundsMin()
{
    return kCenter - Float3{ kBoundsHalf, kBoundsHalf, kBoundsHalf };
}

Float3 boundsMax()
{
    return kCenter + Float3{ kBoundsHalf, kBoundsHalf, kBoundsHalf };
}

float sdf(const Float3& p)
{
    const Float3 c = p - kCenter;
    // 角のとがった箱から、 +x+y+z の角を球で削る (削る = max(a, −b)。 02 の opSubtract と同じ)
    const float box = sdBox(c, { kBoxHalf, kBoxHalf, kBoxHalf });
    const float scoop = sdSphere(c - Float3{ kBoxHalf, kBoxHalf, kBoxHalf }, kScoopRadius);
    const float carved = std::max(box, -scoop);
    // 細い輪を足す (足す = min。 02 の opUnion と同じ)
    const float ring = sdTorus(c, kRingRadius, kRingThickness);
    return std::min(carved, ring);
}

} // namespace baked_shape
