#include "GraphicsDevice.h"

#include "DxCheck.h"

using Microsoft::WRL::ComPtr;

GraphicsDevice::GraphicsDevice()
{
    UINT factoryFlags = 0;
#ifdef _DEBUG
    // Debug ビルドではデバッグレイヤーを有効にする。
    // 状態の食い違いや解放忘れなど、 API の誤った使い方を出力ウィンドウに知らせてくれる。
    // デバイスを作る前に有効にしないと効かない。
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
        debug->EnableDebugLayer();
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif
    ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&mFactory)), "CreateDXGIFactory2");

    // アダプター (GPU) に nullptr を渡すと、 既定の GPU を使う。
    // 機能レベル 11_0 は DirectX 12 が動く最低限。 この描画には特別な機能は要らない。
    ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice)), "D3D12CreateDevice");

    // DIRECT のキューは描画・計算・コピーのどの命令も受け付ける。 この描画には 1 本で足りる。
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mQueue)), "CreateCommandQueue");

    // フェンスは 0 から始め、 待つたびに 1 ずつ大きい値を Signal する (waitForGpu)。
    ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)), "CreateFence");

    // 自動では戻らない (FALSE)、 最初は立っていない (FALSE) イベント
    mFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!mFenceEvent) ThrowLastError("CreateEvent");
}

GraphicsDevice::~GraphicsDevice()
{
    if (mFenceEvent) CloseHandle(mFenceEvent);
}

void GraphicsDevice::execute(ID3D12CommandList* list)
{
    mQueue->ExecuteCommandLists(1, &list);
}

void GraphicsDevice::waitForGpu()
{
    // 1. キューの最後に 「フェンスを mFenceValue にせよ」 という命令を足す。
    //    GPU は、 それより前に送られた命令を全部終えてから、 この命令を実行する。
    ++mFenceValue;
    ThrowIfFailed(mQueue->Signal(mFence.Get(), mFenceValue), "Signal");

    // 2. フェンスがまだその値に届いていなければ、 届いたときにイベントを立ててもらい、 それを待つ。
    //    (もう届いていれば待たずに戻る)
    if (mFence->GetCompletedValue() < mFenceValue) {
        ThrowIfFailed(mFence->SetEventOnCompletion(mFenceValue, mFenceEvent), "SetEventOnCompletion");
        WaitForSingleObject(mFenceEvent, INFINITE);
    }
}
