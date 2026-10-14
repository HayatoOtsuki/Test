#pragma once
// =============================================================================
// Renderer.h — フレームの始まりと終わり
//
//   beginFrame: 記録を始め、 バックバッファと深度をクリアして背景 (空と床) を描く。
//               部品の draw3D へ渡す DrawContext を返す
//   endFrame  : 記録を閉じて GPU へ送り、 表示して、 GPU が終えるのを待つ
//
// その間に Application が部品の draw3D を呼ぶ。
// =============================================================================

#include "BackgroundPass.h"
#include "DepthBuffer.h"
#include "DrawContext.h"
#include "GraphicsDevice.h"
#include "SwapChain.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <string>

class OrbitCamera;

class Renderer {
public:
    // shaderRoot: エンジンのシェーダーのフォルダ (Background.hlsl がある所)
    Renderer(HWND hwnd, UINT width, UINT height, const std::wstring& shaderRoot);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    DrawContext beginFrame(const OrbitCamera& camera, float time);
    void endFrame();

    // 描画先の大きさを合わせる。 変わっていなければ何もしない
    void resize(UINT width, UINT height);

    // 背景のシェーダーを読み直す。 失敗したら false と理由を返し、 前のシェーダーで描き続ける
    bool reloadShaders(std::string& error);

    // 部品が初期化に使う物
    GraphicsDevice& getGraphicsDevice() { return mDevice; }
    DXGI_FORMAT getColorFormat() const { return SwapChain::kFormat; }
    DXGI_FORMAT getDepthFormat() const { return DepthBuffer::kFormat; }

private:
    // リソースの状態を before から after へ移す命令を記録する
    void transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

    // メンバーは宣言の順に作られ、 逆の順に壊される。 ほかが使うデバイスを最初に置く
    GraphicsDevice mDevice;
    SwapChain mSwapChain;
    DepthBuffer mDepthBuffer;
    BackgroundPass mBackground;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mAllocator;      // コマンドを書き込むメモリ
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;  // コマンドを記録する物
};
