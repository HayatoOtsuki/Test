#pragma once
// =============================================================================
// DisplayPass.h — フレームの最後に 1 回だけ、 表示用の色に直す
//
// シーン (SceneColorBuffer、 明るさのまま) を全画面の三角形で読み、 ToDisplay (トーンマップとガンマ) で
// 表示用の色にしてバックバッファへ写す。 各シェーダーが表示用の変換をしなくてよいのは、 ここで 1 回だけするため。
// =============================================================================

#include <d3d12.h>
#include <wrl/client.h>

#include <string>

class DisplayPass {
public:
    // outputFormat: バックバッファの形式。 shaderPath: Display.hlsl の場所
    DisplayPass(ID3D12Device* device, DXGI_FORMAT outputFormat, std::wstring shaderPath);

    // シェーダーをコンパイルして PSO を作り直す。 失敗したら false と理由を返し、 前の PSO を残す
    bool build(std::string& error);

    // srvHeap / sceneColor: シーンの描画先を読むための SRV とそのヒープ
    void draw(ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* srvHeap,
              D3D12_GPU_DESCRIPTOR_HANDLE sceneColor);

private:
    void createRootSignature();

    ID3D12Device* mDevice;
    DXGI_FORMAT mOutputFormat;
    std::wstring mShaderPath;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
};
