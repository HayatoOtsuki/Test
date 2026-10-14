#pragma once
// =============================================================================
// MathTypes.h — カメラと物体の位置に使う、 小さなベクトルと行列
//
// 約束 (DirectX の昔からの流儀に合わせる):
//   ・行ベクトル: 点 p を行列 M で変えるときは p × M と書く。 変換をつなぐときは左から順に掛ける
//     (ワールド → クリップ空間は view × proj)。
//   ・行列は行優先で m[行][列] に入れる。
//   ・座標は y が上の右手系。 カメラは -z を向く。 射影後の奥行きは 0 (手前) 〜 1 (奥)。
//
// HLSL の定数は列優先で読まれるので、 シェーダーへ渡すときは transpose してから入れる。
// そうすると HLSL では mul(float4(p, 1), M) と、 CPU と同じ順で書ける。
// =============================================================================

#include <cmath>

struct Float3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline Float3 operator+(Float3 a, Float3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Float3 operator-(Float3 a, Float3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Float3 operator*(Float3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }

inline float dot(Float3 a, Float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// 右手の法則: a から b へ回すとき、 右手の親指が向く方
inline Float3 cross(Float3 a, Float3 b)
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline Float3 normalize(Float3 a) { return a * (1.0f / std::sqrt(dot(a, a))); }

struct Float4x4 {
    float m[4][4] = {};
};

inline Float4x4 identity()
{
    Float4x4 r;
    for (int i = 0; i < 4; ++i) r.m[i][i] = 1.0f;
    return r;
}

// a × b (先に a、 次に b の変換をする行列)
inline Float4x4 operator*(const Float4x4& a, const Float4x4& b)
{
    Float4x4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k) r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}

inline Float4x4 transpose(const Float4x4& a)
{
    Float4x4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) r.m[i][j] = a.m[j][i];
    return r;
}

// 逆行列 (余因子を並べて行列式で割る)。 逆が無い行列 (行列式が 0) には使わないこと
inline Float4x4 inverse(const Float4x4& a)
{
    const float* m = &a.m[0][0];  // 16 個を 1 列に並べて読む (m[4 * 行 + 列])
    float inv[16];

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    // 行列式 = 1 行目と、 その余因子の積の和
    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    Float4x4 r;
    for (int i = 0; i < 16; ++i) (&r.m[0][0])[i] = inv[i] / det;
    return r;
}

// ワールド → ビュー空間 (カメラが原点で -z を向き、 +x が右、 +y が上)。
// 3 本の軸 (右・上・後ろ) を列に並べ、 最後の行でカメラの位置を引く
inline Float4x4 lookAtRH(Float3 eye, Float3 target, Float3 up)
{
    const Float3 zAxis = normalize(eye - target);       // 後ろ (カメラは -z を向くので)
    const Float3 xAxis = normalize(cross(up, zAxis));   // 右
    const Float3 yAxis = cross(zAxis, xAxis);           // 上
    Float4x4 r;
    r.m[0][0] = xAxis.x; r.m[0][1] = yAxis.x; r.m[0][2] = zAxis.x;
    r.m[1][0] = xAxis.y; r.m[1][1] = yAxis.y; r.m[1][2] = zAxis.y;
    r.m[2][0] = xAxis.z; r.m[2][1] = yAxis.z; r.m[2][2] = zAxis.z;
    r.m[3][0] = -dot(xAxis, eye);
    r.m[3][1] = -dot(yAxis, eye);
    r.m[3][2] = -dot(zAxis, eye);
    r.m[3][3] = 1.0f;
    return r;
}

// ビュー空間 → クリップ空間の透視投影。
//   fovY  : 縦の画角 (ラジアン)
//   aspect: 画面の横 ÷ 縦
//   nearZ, farZ: 描く奥行きの範囲 (メートル)。 nearZ が奥行き 0、 farZ が奥行き 1 になる
inline Float4x4 perspectiveFovRH(float fovY, float aspect, float nearZ, float farZ)
{
    const float yScale = 1.0f / std::tan(fovY * 0.5f);
    const float xScale = yScale / aspect;
    const float range = farZ / (nearZ - farZ);
    Float4x4 r;
    r.m[0][0] = xScale;
    r.m[1][1] = yScale;
    r.m[2][2] = range;
    r.m[2][3] = -1.0f;  // w = -z (カメラの前にある点ほど z は負なので、 w は正になる)
    r.m[3][2] = range * nearZ;
    return r;
}
