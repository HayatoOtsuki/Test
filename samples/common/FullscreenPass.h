#pragma once
// =============================================================================
// FullscreenPass.h — 画面全体を 1 枚の三角形で描くサンプル用のパイプライン
//
// SDF の断面・レイマーチング・球追跡のサンプルは、 メッシュを使わず、 画素ごとに光線を作って絵を描く。
// その 「全画面の三角形を描く」 部分を、 ここにまとめる。
//
// シェーダー (samples/common/shaders/Fullscreen.hlsli を #include した物) に渡すもの:
//   b0: エンジンのシーンの定数 (カメラ・ライト)       — ルートの定数バッファビュー
//   b1: サンプルの定数 (32 bit × constantCount 個)    — ルート定数
//   t0: テクスチャ 1 つ (withTexture のときだけ)       — デスクリプターテーブル + 静的サンプラー s0
// =============================================================================

#include "engine/DrawContext.h"
#include "engine/RenderEntity.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <string>

class FullscreenPass {
public:
    // shaderFile   : VSMain と PSMain を持つ HLSL のファイル
    // constantCount: b1 に置く 32 bit の値の数 (1 以上)
    // withTexture  : t0 のテクスチャを使うか
    FullscreenPass(std::wstring shaderFile, UINT constantCount, bool withTexture = false);

    // ルートシグネチャと PSO を作る (サンプルの initializeImplement から呼ぶ)
    void initialize(const RenderSetup& setup);

    // シェーダーを読み直す。 失敗したら false と理由を返し、 前の PSO のまま
    bool reload(std::string& error);

    // 全画面の三角形を描く。 constants: constantCount 個の 32 bit の値。
    // heap / texture: withTexture のとき、 SRV があるヒープ (シェーダーから見える物) と、 その SRV
    void draw(const DrawContext& context, const void* constants, ID3D12DescriptorHeap* heap = nullptr,
              D3D12_GPU_DESCRIPTOR_HANDLE texture = {});

private:
    std::wstring mShaderFile;
    UINT mConstantCount;
    bool mWithTexture;
    ID3D12Device* mDevice = nullptr;
    RenderSetup mSetup;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipeline;
};
