#pragma once
// =============================================================================
// GraphicsDevice.h — GPU とのやりとりの土台
//
//   DXGI ファクトリー : スワップチェーンを作るときに使う
//   デバイス         : GPU 上の物 (リソース・パイプラインなど) を作る窓口
//   コマンドキュー   : 記録したコマンドリストを GPU へ送る列
//   フェンス         : GPU がどこまで終えたかを CPU が知るための数
//
// DirectX 12 では、 CPU が送った命令を GPU がいつ終えるかを自分で管理する。
// ここでは簡単さを優先し、 毎フレーム waitForGpu で GPU が終えるのを待つ。
// =============================================================================

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

class GraphicsDevice {
public:
    GraphicsDevice();
    ~GraphicsDevice();

    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    // 記録を終えたコマンドリストを GPU へ送る。 送るだけで、 終わるのは待たない
    void execute(ID3D12CommandList* list);

    // これまでに送った命令を、 GPU が全部終えるまで CPU を止める
    void waitForGpu();

    ID3D12Device* getDevice() const { return mDevice.Get(); }
    ID3D12CommandQueue* getQueue() const { return mQueue.Get(); }
    IDXGIFactory4* getFactory() const { return mFactory.Get(); }

private:
    Microsoft::WRL::ComPtr<IDXGIFactory4> mFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mQueue;
    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    UINT64 mFenceValue = 0;        // 最後に Signal した値
    HANDLE mFenceEvent = nullptr;  // フェンスがその値に届いたら立ててもらうイベント
};
