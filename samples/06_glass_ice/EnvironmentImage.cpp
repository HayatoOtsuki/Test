#include "EnvironmentImage.h"

#include <algorithm>
#include <cmath>

namespace {

float saturate(float x)
{
    return std::min(std::max(x, 0.0f), 1.0f);
}

Float3 lerp(const Float3& a, const Float3& b, float t)
{
    return a + (b - a) * t;
}

Float3 mulColor(const Float3& a, const Float3& b)
{
    return { a.x * b.x, a.y * b.y, a.z * b.z };
}

// 空: 地平線から天頂へのグラデーション + 太陽の円盤とまわりの光 (Environment.hlsli の Sky と同じ式)
Float3 sky(const Float3& rd, const Float3& lightDirection, const Float3& lightColor)
{
    const Float3 horizon = { 0.62f, 0.72f, 0.85f };
    const Float3 zenith = { 0.16f, 0.34f, 0.70f };
    const Float3 col = lerp(horizon, zenith, saturate(rd.y * 1.6f));
    const float s = std::max(dot(rd, lightDirection), 0.0f);
    return col + lightColor * (std::pow(s, 900.0f) * 6.0f + std::pow(s, 10.0f) * 0.12f);
}

// 床: 50 cm の市松模様に、 太陽の光と空の光を当てる (Environment.hlsli の ShadeFloor と同じ式)
Float3 shadeFloor(const Float3& p, const Float3& lightDirection, const Float3& lightColor)
{
    const float sum = std::floor(p.x * 2.0f) + std::floor(p.z * 2.0f);
    const float checker = (sum * 0.5f - std::floor(sum * 0.5f)) * 2.0f;  // frac(sum × 0.5) × 2: 偶数なら 0、 奇数なら 1
    const Float3 albedo = lerp({ 0.46f, 0.44f, 0.40f }, { 0.22f, 0.23f, 0.26f }, checker);
    const Float3 light = lightColor * saturate(lightDirection.y) + Float3{ 0.22f, 0.30f, 0.42f };
    return mulColor(albedo, light);
}

} // namespace

namespace environment_image {

Float3 probePosition()
{
    return { 0.0f, 0.4f, 0.0f };
}

Float3 environment(const Float3& ro, const Float3& rd, const Float3& lightDirection, const Float3& lightColor)
{
    if (rd.y < 0.0f) {
        // 床 (y = 0) に当たる。 遠いほど空の色に近づける (霧)
        const float t = std::max(-ro.y / rd.y, 0.0f);
        const Float3 floorColor = shadeFloor(ro + rd * t, lightDirection, lightColor);
        return lerp(floorColor, sky(rd, lightDirection, lightColor), 1.0f - std::exp(-t * 0.03f));
    }
    return sky(rd, lightDirection, lightColor);
}

Float3 faceDirection(int face, int x, int y, int size)
{
    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f;
    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f;
    Float3 d;
    switch (face) {
    case 0: d = { 1.0f, -v, -u }; break;   // +X
    case 1: d = { -1.0f, -v, u }; break;   // −X
    case 2: d = { u, 1.0f, v }; break;     // +Y
    case 3: d = { u, -1.0f, -v }; break;   // −Y
    case 4: d = { u, -v, 1.0f }; break;    // +Z
    default: d = { -u, -v, -1.0f }; break; // −Z
    }
    return normalize(d);
}

std::vector<float> makeFaces(int size, const Float3& lightDirection, const Float3& lightColor)
{
    std::vector<float> rgba(static_cast<size_t>(6) * size * size * 4);
    const Float3 probe = probePosition();
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const Float3 c = environment(probe, faceDirection(face, x, y, size), lightDirection, lightColor);
                float* out = &rgba[((static_cast<size_t>(face) * size + y) * size + x) * 4];
                out[0] = c.x;
                out[1] = c.y;
                out[2] = c.z;
                out[3] = 1.0f;
            }
        }
    }
    return rgba;
}

} // namespace environment_image
