#pragma once
// =============================================================================
// OrbitCamera.h — 注視点の周りを回るカメラ
//
// 持つのは 3 つの数だけ:
//   yaw      : 水平の角度 (ラジアン)。 0 で +z の側から注視点を見る
//   pitch    : 見下ろす角度 (ラジアン)。 0 で水平、 正で上から
//   distance : 注視点からの距離 (メートル)
// ここからカメラの位置と、 ビュー・射影の行列を作る。 座標は y が上の右手系。
// =============================================================================

#include <DirectXMath.h>

class OrbitCamera {
public:
    // マウスの移動量 (画素) で回す。 横で水平に回り込み、 縦で見下ろす角度が変わる
    void rotate(int dx, int dy);

    // ホイールの目盛りの数で寄る・引く (正で寄る)。 1 目盛りで距離が 0.9 倍
    void zoom(float notches);

    // 時間を dt 秒進める。 自動回転が有効なら水平にゆっくり回る
    void update(float dt);

    void toggleAutoRotate() { mAutoRotate = !mAutoRotate; }
    void stopAutoRotate() { mAutoRotate = false; }

    // カメラの位置 (ワールド)
    DirectX::XMFLOAT3 getPosition() const;

    // ワールド → ビュー空間 (カメラが原点で -z を向く)
    DirectX::XMMATRIX getViewMatrix() const;

    // ビュー空間 → クリップ空間。 aspect は画面の横 ÷ 縦
    DirectX::XMMATRIX getProjectionMatrix(float aspect) const;

private:
    DirectX::XMFLOAT3 mTarget = { 0.0f, 0.4f, 0.0f };  // 注視点 (氷の真ん中あたり)
    float mYaw = 0.6f;
    float mPitch = 0.35f;
    float mDistance = 3.0f;
    bool mAutoRotate = true;
};
