#include "Renderer.h"

#include "DxCheck.h"
#include "OrbitCamera.h"

#include <algorithm>
#include <stdexcept>

Renderer::Renderer(HWND hwnd, UINT width, UINT height, const std::wstring& shaderRoot)
    : mSwapChain(mDevice, hwnd, width, height)
    , mDepthBuffer(mDevice.getDevice(), width, height)
    , mSceneColor(mDevice.getDevice(), width, height)
    , mSceneCopy(mDevice.getDevice(), width, height)
    , mBackground(mDevice.getDevice(), SceneColorBuffer::kFormat, DepthBuffer::kFormat, shaderRoot + L"/Background.hlsl")
    , mDisplay(mDevice.getDevice(), SwapChain::kFormat, shaderRoot + L"/Display.hlsl")
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

    createSceneConstantBuffer();

    std::string error;
    if (!mBackground.build(error)) throw std::runtime_error("Background shader error:\n" + error);
    if (!mDisplay.build(error)) throw std::runtime_error("Display shader error:\n" + error);
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

void Renderer::createSceneConstantBuffer()
{
    // 定数バッファの大きさは 256 バイトの倍数にする決まり (SceneConstants は 176 バイトなので 256)
    const UINT64 size = (sizeof(SceneConstants) + 255) & ~UINT64(255);

    // CPU から毎フレーム書くので、 アップロードヒープに置く
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ThrowIfFailed(mDevice.getDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                               D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                               IID_PPV_ARGS(&mSceneBuffer)),
                  "CreateCommittedResource(scene constants)");

    // Map したままにしておき、 毎フレーム書く。 アップロードヒープは Map したままでよい。
    // 毎フレーム GPU が終えるのを待っているので、 書いている間に GPU が読むことはない
    const D3D12_RANGE noRead = { 0, 0 };
    ThrowIfFailed(mSceneBuffer->Map(0, &noRead, reinterpret_cast<void**>(&mSceneMapped)), "Map(scene constants)");
}

void Renderer::addEntity(RenderEntity* entity)
{
    RenderSetup setup;
    setup.device = mDevice.getDevice();
    setup.colorFormat = SceneColorBuffer::kFormat;  // 描く物は全部、 シーンの描画先 (明るさのまま) へ描く
    setup.depthFormat = DepthBuffer::kFormat;
    entity->initializeImplement(setup);  // 失敗したら例外が出て、 登録されない
    mEntities.push_back(entity);
}

void Renderer::removeEntity(RenderEntity* entity)
{
    // GPU が描いている途中の物を外さないよう、 終わるのを待つ
    mDevice.waitForGpu();
    mEntities.erase(std::remove(mEntities.begin(), mEntities.end(), entity), mEntities.end());
}

void Renderer::renderFrame(const OrbitCamera& camera, const DirectionalLight& light, float time)
{
    const DrawContext context = beginFrame(camera, light, time);  // 背景まで描く
    for (RenderEntity* entity : mEntities) entity->drawImplement(context);
    endFrame();
}

DrawContext Renderer::beginFrame(const OrbitCamera& camera, const DirectionalLight& light, float time)
{
    // 1. 記録の準備。 前のフレームは waitForGpu で終わっているので、 アロケーターの中身を捨ててよい
    ThrowIfFailed(mAllocator->Reset(), "CommandAllocator::Reset");
    ThrowIfFailed(mCommandList->Reset(mAllocator.Get(), nullptr), "CommandList::Reset");

    // 2. シーンの描画先を 「シェーダーから読む」 から 「描画先」 の状態へ移す (前のフレームの最後に読んだため)
    transition(mSceneColor.getResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // 3. 描画先 (シーンの色と深度) をつなぎ、 深度を 1.0 (いちばん奥) でクリアする。
    //    色は背景がすべての画素を塗るのでクリアしない
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = mSceneColor.getRtv();  // シーンは明るさのまま描く (表示用の変換は endFrame で 1 回だけ)
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

    // 6. シーンの定数 (カメラとライト) を定数バッファに書く。 どのシェーダーも b0 でこれを読む。
    //    HLSL の定数は列優先で読まれるので、 行列は転置して渡す (HLSL では mul(float4(p, 1), M) と書ける)
    SceneConstants scene = {};
    scene.viewProj = transpose(viewProj);
    scene.invViewProj = transpose(inverse(viewProj));  // 背景で画素ごとの光線を作るのに使う
    scene.cameraPos = camera.getPosition();
    scene.time = time;
    scene.lightDirection = normalize(light.direction);
    scene.lightColor = light.color * light.intensity;
    *mSceneMapped = scene;

    DrawContext context;
    context.commandList = mCommandList.Get();
    context.sceneConstants = mSceneBuffer->GetGPUVirtualAddress();
    context.scene = scene;

    // 7. 背景 (空と床)。 深度には書かないので、 後から描く物がいつも手前に出る
    mBackground.draw(mCommandList.Get(), context.sceneConstants);

    // 8. 背景を描いた画面をコピーしておく。 透ける物 (氷) が、 屈折した先の周りの景色をここから読む。
    //    コピーの間だけ、 シーンの描画先を 「コピー元」、 コピー先を 「コピー先」 の状態にする
    transition(mSceneColor.getResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    transition(mSceneCopy.getResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    mCommandList->CopyResource(mSceneCopy.getResource(), mSceneColor.getResource());
    transition(mSceneCopy.getResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(mSceneColor.getResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    context.sceneCopy = mSceneCopy.getResource();
    return context;
}

void Renderer::endFrame()
{
    // 1. シーンを描き終えたので、 シーンの描画先を 「シェーダーから読む」 状態へ移す
    transition(mSceneColor.getResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // 2. バックバッファを描画先にして、 シーンを 1 回だけ表示用の色に直して写す (ToDisplay: トーンマップとガンマ)。
    //    ビューポートとシザーは beginFrame で決めた物 (同じ大きさ) のまま
    transition(mSwapChain.getCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const D3D12_CPU_DESCRIPTOR_HANDLE backBuffer = mSwapChain.getCurrentRtv();
    mCommandList->OMSetRenderTargets(1, &backBuffer, FALSE, nullptr);
    mDisplay.draw(mCommandList.Get(), mSceneColor.getSrvHeap(), mSceneColor.getSrv());

    // 3. 描き終えたら 「表示用」 の状態へ戻し、 記録を閉じる
    transition(mSwapChain.getCurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    ThrowIfFailed(mCommandList->Close(), "CommandList::Close");

    // 4. GPU へ送る → 表示に回す → GPU が終えるまで待つ
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
    mSceneColor.resize(width, height);
    mSceneCopy.resize(width, height);
}

bool Renderer::reloadShaders(std::string& error)
{
    // 使っている途中の PSO を差し替えないよう、 GPU が終えるのを待つ
    mDevice.waitForGpu();
    if (!mBackground.build(error)) return false;
    if (!mDisplay.build(error)) return false;
    for (RenderEntity* entity : mEntities) {
        if (!entity->reloadImplement(error)) return false;
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
