#pragma once
// =============================================================================
// BackgroundPass.h — 背景 (空と床) を全画面に描く
//
// 画面を覆う三角形を 1 枚描き、 ピクセルシェーダーで画素ごとにカメラからの光線を作って、
// その光線の先の色 (shaders/Environment.hlsli の Environment) を塗る。
// カメラとライトはシーンの定数 (b0) から読む。 深度には書かない (いちばん奥にある物として扱う)。
// =============================================================================

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

    // sceneConstants: シーンの定数バッファの GPU のアドレス (b0 に結び付ける)
    void draw(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS sceneConstants);

private:
    void createRootSignature();

    ID3D12Device* mDevice;
    DXGI_FORMAT mColorFormat;
    DXGI_FORMAT mDepthFormat;
    std::wstring mShaderPath;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
};
