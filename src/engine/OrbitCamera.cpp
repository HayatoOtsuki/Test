#include "OrbitCamera.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kRadiansPerPixel = 0.008f;  // マウス 1 画素で回る角度
constexpr float kMinPitch = -0.1f;          // 少しだけ下から見上げられる
constexpr float kMaxPitch = 1.45f;          // 真上 (π/2) の手前で止める。 真上だと上の向きが決まらない
constexpr float kMinDistance = 1.0f;
constexpr float kMaxDistance = 12.0f;
constexpr float kZoomPerNotch = 0.9f;       // 1 目盛りで距離を何倍にするか
constexpr float kAutoRotateSpeed = 0.15f;   // 自動回転の速さ (ラジアン / 秒)

// 縦の画角。 縦 1 の画面を前 1.5 の所に置いたときの角度 2 × atan(0.5 / 1.5) ≈ 37°
const float kFovY = 2.0f * std::atan(0.5f / 1.5f);
constexpr float kNearZ = 0.05f;  // これより手前は描かない (メートル)
constexpr float kFarZ = 100.0f;  // これより奥は描かない

} // namespace

void OrbitCamera::rotate(int dx, int dy)
{
    // 右へドラッグすると、 つかんだ物を右へ回すように見える向きに回る
    mYaw -= static_cast<float>(dx) * kRadiansPerPixel;
    // 下へドラッグすると、 上から見下ろす側へ
    mPitch = std::clamp(mPitch + static_cast<float>(dy) * kRadiansPerPixel, kMinPitch, kMaxPitch);
}

void OrbitCamera::zoom(float notches)
{
    // 掛け算で変えると、 近くでも遠くでも 1 目盛りの手応えが同じになる
    mDistance = std::clamp(mDistance * std::pow(kZoomPerNotch, notches), kMinDistance, kMaxDistance);
}

void OrbitCamera::update(float dt)
{
    if (mAutoRotate) mYaw += dt * kAutoRotateSpeed;
}

Float3 OrbitCamera::getPosition() const
{
    // 注視点を中心とする球の上の点 (球面座標)。
    // yaw = 0 で +z の側、 yaw を増やすと +x の側へ回り込む。 pitch を増やすと上へ
    const float c = std::cos(mPitch);
    return { mTarget.x + mDistance * c * std::sin(mYaw),
             mTarget.y + mDistance * std::sin(mPitch),
             mTarget.z + mDistance * c * std::cos(mYaw) };
}

Float4x4 OrbitCamera::getViewMatrix() const
{
    // RH = 右手系。 カメラは -z を向き、 +x が右、 +y が上になる
    return lookAtRH(getPosition(), mTarget, { 0.0f, 1.0f, 0.0f });
}

Float4x4 OrbitCamera::getProjectionMatrix(float aspect) const
{
    // 奥行きを 0 (kNearZ) 〜 1 (kFarZ) に写す透視投影
    return perspectiveFovRH(kFovY, aspect, kNearZ, kFarZ);
}
