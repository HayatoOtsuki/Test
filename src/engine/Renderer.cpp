#include "Renderer.h"

#include "DxCheck.h"
#include "OrbitCamera.h"

#include <algorithm>
#include <stdexcept>

Renderer::Renderer(HWND hwnd, UINT width, UINT height, const std::wstring& shaderRoot)
    : mSwapChain(mDevice, hwnd, width, height)
    , mDepthBuffer(mDevice.getDevice(), width, height)
    , mBackground(mDevice.getDevice(), SwapChain::kFormat, DepthBuffer::kFormat, shaderRoot + L"/Background.hlsl")
{
    ID3D12Device* device = mDevice.getDevice();

    // コマンドアロケーターとコマンドリストは 1 組だけ。
    // 毎フレーム GPU が終えるのを待つので、 次のフレームでそのまま使い回せる。
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mAllocator)),
                  "CreateCommandAllocator");
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mAllocator.Get(), nullptr,
                                            IID_PPV_ARGS(&mCommandList)),
                  "CreateCommandList");
    // 作ったばかりのコマンドリストは記録中の状態。 beginFrame で Reset できるよう、 いったん閉じる
    ThrowIfFailed(mCommandList->Close(), "CommandList::Close");

    std::string error;
    if (!mBackground.build(error)) throw std::runtime_error("Background shader error:\n" + error);
}

Renderer::~Renderer()
{
    // GPU が使っている途中のリソースを壊さないよう、 全部終わるのを待ってから片付ける。
    // デストラクターから例外を出さないよう、 ここでは失敗を無視する
    try {
        mDevice.waitForGpu();
    } catch (...) {
    }
}

void Renderer::addEntity(RenderEntity* entity)
{
    RenderSetup setup;
    setup.device = mDevice.getDevice();
    setup.colorFormat = SwapChain::kFormat;
    setup.depthFormat = DepthBuffer::kFormat;
    entity->initialize(setup);  // 失敗したら例外が出て、 登録されない
    mEntities.push_back(entity);
}

void Renderer::removeEntity(RenderEntity* entity)
{
    // GPU が描いている途中の物を外さないよう、 終わるのを待つ
    mDevice.waitForGpu();
    mEntities.erase(std::remove(mEntities.begin(), mEntities.end(), entity), mEntities.end());
}

void Renderer::renderFrame(const OrbitCamera& camera, float time)
{
    const DrawContext context = beginFrame(camera, time);  // 背景まで描く
    for (RenderEntity* entity : mEntities) entity->draw3D(context);
    endFrame();
}

DrawContext Renderer::beginFrame(const OrbitCamera& camera, float time)
{
    // 1. 記録の準備。 前のフレームは waitForGpu で終わっているので、 アロケーターの中身を捨ててよい
    ThrowIfFailed(mAllocator->Reset(), "CommandAllocator::Reset");
    ThrowIfFailed(mCommandList->Reset(mAllocator.Get(), nullptr), "CommandList::Reset");

    // 2. バックバッファを 「表示用 (PRESENT)」 から 「描画先 (RENDER_TARGET)」 の状態へ移す
    transition(mSwapChain.getCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // 3. 描画先 (色と深度) をつなぎ、 深度を 1.0 (いちばん奥) でクリアする。
    //    色は背景がすべての画素を塗るのでクリアしない
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = mSwapChain.getCurrentRtv();
    const D3D12_CPU_DESCRIPTOR_HANDLE dsv = mDepthBuffer.getDsv();
    mCommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    mCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // 4. 描く範囲 (ビューポート = 座標の変換、 シザー = 切り抜き) を画面全体にする
    const float width = static_cast<float>(mSwapChain.getWidth());
    const float height = static_cast<float>(mSwapChain.getHeight());
    const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, width, height, 0.0f, 1.0f };
    const D3D12_RECT scissor = { 0, 0, static_cast<LONG>(mSwapChain.getWidth()),
                                 static_cast<LONG>(mSwapChain.getHeight()) };
    mCommandList->RSSetViewports(1, &viewport);
    mCommandList->RSSetScissorRects(1, &scissor);

    // 5. カメラの行列。 行ベクトル (p × M) の流儀なので、 ワールド → クリップは view × proj (MathTypes.h)
    const Float4x4 viewProj = camera.getViewMatrix() * camera.getProjectionMatrix(width / height);

    // HLSL の定数は列優先で読まれるので、 転置して渡す (HLSL では mul(float4(p, 1), M) と書ける)
    DrawContext context;
    context.commandList = mCommandList.Get();
    context.viewProj = transpose(viewProj);
    context.cameraPos = camera.getPosition();
    context.time = time;

    // 6. 背景 (空と床)。 画素ごとの光線を作るため、 逆行列 (クリップ → ワールド) を渡す。
    //    深度には書かないので、 後から描く物がいつも手前に出る
    mBackground.draw(mCommandList.Get(), transpose(inverse(viewProj)), context.cameraPos, time);
    return context;
}

void Renderer::endFrame()
{
    // 1. 描き終えたら 「表示用」 の状態へ戻し、 記録を閉じる
    transition(mSwapChain.getCurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    ThrowIfFailed(mCommandList->Close(), "CommandList::Close");

    // 2. GPU へ送る → 表示に回す → GPU が終えるまで待つ
    mDevice.execute(mCommandList.Get());
    mSwapChain.present();
    mDevice.waitForGpu();
}

void Renderer::resize(UINT width, UINT height)
{
    if (width == mSwapChain.getWidth() && height == mSwapChain.getHeight()) return;
    // バックバッファと深度バッファを作り直すので、 GPU が使い終わるのを待つ
    mDevice.waitForGpu();
    mSwapChain.resize(width, height);
    mDepthBuffer.resize(width, height);
}

bool Renderer::reloadShaders(std::string& error)
{
    // 使っている途中の PSO を差し替えないよう、 GPU が終えるのを待つ
    mDevice.waitForGpu();
    if (!mBackground.build(error)) return false;
    for (RenderEntity* entity : mEntities) {
        if (!entity->reloadShaders(error)) return false;
    }
    return true;
}

void Renderer::transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    // リソースバリア: 「このリソースの使い方が変わる」 と GPU に伝える。
    // GPU はそれまでの書き込みを終えてから、 次の使い方に合わせた準備をする
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    mCommandList->ResourceBarrier(1, &barrier);
}
