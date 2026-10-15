#pragma once
// =============================================================================
// GpuHelpers.h — サンプルがよく使う GPU の小さな道具
//
// どのサンプルにも出てくる 「決まった手順」 をまとめ、 サンプルのコードを、 そのサンプルの機能だけにする。
// 中身の説明は本体の解説 (docs/03-ice-component.md の 「GPU のバッファ」「パイプライン」) と同じ。
// =============================================================================

#include "engine/RenderEntity.h"  // RenderSetup

#include <d3d12.h>
#include <wrl/client.h>

#include <string>

namespace gpu {

// CPU から書けて GPU から読めるバッファ (アップロードヒープ) を作り、 data を写す。 data が nullptr なら写さない
Microsoft::WRL::ComPtr<ID3D12Resource> createUploadBuffer(ID3D12Device* device, const void* data, size_t size);

// ルートシグネチャの記述を、 バイト列に直して (Serialize) 作る
Microsoft::WRL::ComPtr<ID3D12RootSignature> createRootSignature(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC& desc);

// 線形補間・端で止める静的サンプラー (s0、 ピクセルシェーダーから見える)
D3D12_STATIC_SAMPLER_DESC linearClampSampler();

// PSO の作り方の違い。 決まった所 (ブレンドなし・反時計回りが表・描画先の形式) は createPipeline が埋める
struct PipelineOptions {
    const D3D12_INPUT_ELEMENT_DESC* inputLayout = nullptr;  // 頂点バッファを読まないなら nullptr
    UINT inputCount = 0;
    D3D12_FILL_MODE fill = D3D12_FILL_MODE_SOLID;           // WIREFRAME にすると三角形の辺だけ描く
    D3D12_CULL_MODE cull = D3D12_CULL_MODE_BACK;            // 描かない面 (三角形は外から見て反時計回りが表)
    bool depthTest = true;                                  // 手前の物だけを残すか
    bool depthWrite = true;                                 // 深度に書くか
};

// file の VSMain と PSMain をコンパイルして PSO を作る。
// 失敗したら false を返し、 error に理由を入れる (pipeline は前の物のまま)
bool createPipeline(ID3D12Device* device, ID3D12RootSignature* rootSignature, const RenderSetup& setup,
                    const std::wstring& file, const PipelineOptions& options,
                    Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipeline, std::string& error);

} // namespace gpu
