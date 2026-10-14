#pragma once
// =============================================================================
// BackgroundPass.h — 背景 (空と床) を全画面に描く
//
// 画面を覆う三角形を 1 枚描き、 ピクセルシェーダーで画素ごとにカメラからの光線を作って、
// その光線の先の色 (shaders/Environment.hlsli の Environment) を塗る。
// 深度には書かない (いちばん奥にある物として扱う)。
// =============================================================================

#include "MathTypes.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <string>

class BackgroundPass {
public:
    // colorFormat / depthFormat: 描画先の形式。 PSO に書き込む
    // shaderPath               : Background.hlsl の場所
    BackgroundPass(ID3D12Device* device, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat, std::wstring shaderPath);

    // シェーダーをコンパイルして PSO を作り直す。 失敗したら false と理由を返し、 前の PSO を残す
    bool build(std::string& error);

    // invViewProj: クリップ空間 → ワールドの行列 (転置済み)。 画素ごとの光線の向きを作るのに使う
    void draw(ID3D12GraphicsCommandList* commandList, const Float4x4& invViewProj, const Float3& cameraPos, float time);

private:
    void createRootSignature();

    ID3D12Device* mDevice;
    DXGI_FORMAT mColorFormat;
    DXGI_FORMAT mDepthFormat;
    std::wstring mShaderPath;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
};
