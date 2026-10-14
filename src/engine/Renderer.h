#pragma once
// =============================================================================
// Renderer.h — 1 フレームの描画
//
// renderFrame の中身:
//   1. beginFrame: 記録を始め、 バックバッファと深度をクリアして背景 (空と床) を描く
//   2. 登録された物 (RenderEntity) の draw3D を順に呼ぶ
//   3. endFrame  : 記録を閉じて GPU へ送り、 表示して、 GPU が終えるのを待つ
// =============================================================================

#include "BackgroundPass.h"
#include "DirectionalLight.h"
#include "DepthBuffer.h"
#include "DrawContext.h"
#include "GraphicsDevice.h"
#include "RenderEntity.h"
#include "SwapChain.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <string>
#include <vector>

class OrbitCamera;

class Renderer {
public:
    // shaderRoot: エンジンのシェーダーのフォルダ (Background.hlsl がある所)
    Renderer(HWND hwnd, UINT width, UINT height, const std::wstring& shaderRoot);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // 描く物を登録する。 ここで entity->initialize を呼び、 以後毎フレーム entity->draw3D を呼ぶ
    void addEntity(RenderEntity* entity);
    // 登録を外す (描く物が壊れる前に呼ぶ)
    void removeEntity(RenderEntity* entity);

    // 1 フレーム描いて表示する。 カメラとライトはシーンの定数バッファに書いて、 全部のシェーダーへ渡す
    void renderFrame(const OrbitCamera& camera, const DirectionalLight& light, float time);

    // 描画先の大きさを合わせる。 変わっていなければ何もしない
    void resize(UINT width, UINT height);

    // 背景と登録された物のシェーダーを読み直す。 失敗したら false と理由を返し、 前のシェーダーで描き続ける
    bool reloadShaders(std::string& error);

private:
    DrawContext beginFrame(const OrbitCamera& camera, const DirectionalLight& light, float time);
    void createSceneConstantBuffer();
    void endFrame();

    // リソースの状態を before から after へ移す命令を記録する
    void transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

    // メンバーは宣言の順に作られ、 逆の順に壊される。 ほかが使うデバイスを最初に置く
    GraphicsDevice mDevice;
    SwapChain mSwapChain;
    DepthBuffer mDepthBuffer;
    BackgroundPass mBackground;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mAllocator;      // コマンドを書き込むメモリ
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;  // コマンドを記録する物
    std::vector<RenderEntity*> mEntities;                           // 登録された描く物 (持ち主は登録した側)

    // シーンの定数 (カメラとライト) を入れる定数バッファ。 アップロードヒープに置き、 ずっと Map したまま書く
    Microsoft::WRL::ComPtr<ID3D12Resource> mSceneBuffer;
    SceneConstants* mSceneMapped = nullptr;
};
