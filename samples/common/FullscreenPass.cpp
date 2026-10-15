#include "common/FullscreenPass.h"

#include "common/GpuHelpers.h"

#include <stdexcept>
#include <utility>

FullscreenPass::FullscreenPass(std::wstring shaderFile, UINT constantCount, bool withTexture)
    : mShaderFile(std::move(shaderFile)), mConstantCount(constantCount), mWithTexture(withTexture)
{
}

void FullscreenPass::initialize(const RenderSetup& setup)
{
    mDevice = setup.device;
    mSetup = setup;

    // ルート引数: 0 = b0 シーンの定数 (CBV)、 1 = b1 サンプルの定数、 2 = t0 テクスチャ (使うときだけ)
    D3D12_DESCRIPTOR_RANGE range = {};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;  // t0

    D3D12_ROOT_PARAMETER params[3] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[1].Constants.ShaderRegister = 1;
    params[1].Constants.Num32BitValues = mConstantCount;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].DescriptorTable.NumDescriptorRanges = 1;
    params[2].DescriptorTable.pDescriptorRanges = &range;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    const D3D12_STATIC_SAMPLER_DESC sampler = gpu::linearClampSampler();
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = mWithTexture ? 3 : 2;
    desc.pParameters = params;
    desc.NumStaticSamplers = mWithTexture ? 1 : 0;
    desc.pStaticSamplers = mWithTexture ? &sampler : nullptr;
    mRootSignature = gpu::createRootSignature(mDevice, desc);

    std::string error;
    if (!reload(error)) throw std::runtime_error("Sample shader error:\n" + error);
}

bool FullscreenPass::reload(std::string& error)
{
    // 頂点バッファは読まない (頂点シェーダーが番号から三角形を作る)。 裏表は気にしない。 深度は使わない
    gpu::PipelineOptions options;
    options.cull = D3D12_CULL_MODE_NONE;
    options.depthTest = false;
    options.depthWrite = false;
    return gpu::createPipeline(mDevice, mRootSignature.Get(), mSetup, mShaderFile, options, mPipeline, error);
}

void FullscreenPass::draw(const DrawContext& context, const void* constants, ID3D12DescriptorHeap* heap,
                          D3D12_GPU_DESCRIPTOR_HANDLE texture)
{
    ID3D12GraphicsCommandList* commandList = context.commandList;
    commandList->SetGraphicsRootSignature(mRootSignature.Get());
    commandList->SetPipelineState(mPipeline.Get());
    commandList->SetGraphicsRootConstantBufferView(0, context.sceneConstants);
    commandList->SetGraphicsRoot32BitConstants(1, mConstantCount, constants, 0);
    if (mWithTexture && heap) {
        ID3D12DescriptorHeap* heaps[] = { heap };
        commandList->SetDescriptorHeaps(1, heaps);
        commandList->SetGraphicsRootDescriptorTable(2, texture);
    }
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
}
