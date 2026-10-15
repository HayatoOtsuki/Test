#include "common/GpuHelpers.h"

#include "engine/DxCheck.h"
#include "engine/ShaderCompiler.h"

#include <cstdio>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace gpu {

ComPtr<ID3D12Resource> createUploadBuffer(ID3D12Device* device, const void* data, size_t size)
{
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;  // バッファは必ずこの並び

    ComPtr<ID3D12Resource> buffer;
    ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                  nullptr, IID_PPV_ARGS(&buffer)),
                  "CreateCommittedResource(upload buffer)");
    if (data) {
        void* mapped = nullptr;
        const D3D12_RANGE noRead = { 0, 0 };  // CPU からは読まない
        ThrowIfFailed(buffer->Map(0, &noRead, &mapped), "Map(upload buffer)");
        std::memcpy(mapped, data, size);
        buffer->Unmap(0, nullptr);
    }
    return buffer;
}

ComPtr<ID3D12RootSignature> createRootSignature(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC& desc)
{
    ComPtr<ID3DBlob> blob, error;
    ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error),
                  "D3D12SerializeRootSignature");
    ComPtr<ID3D12RootSignature> rootSignature;
    ThrowIfFailed(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                              IID_PPV_ARGS(&rootSignature)),
                  "CreateRootSignature");
    return rootSignature;
}

D3D12_STATIC_SAMPLER_DESC linearClampSampler()
{
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;  // 点の間をなめらかにつなぐ
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;  // 外を読んでも端の値
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;  // s0
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    return sampler;
}

bool createPipeline(ID3D12Device* device, ID3D12RootSignature* rootSignature, const RenderSetup& setup,
                    const std::wstring& file, const PipelineOptions& options, ComPtr<ID3D12PipelineState>& pipeline,
                    std::string& error)
{
    std::string log;
    ComPtr<ID3DBlob> vs = ShaderCompiler::compile(file, "VSMain", "vs_5_0", log);
    ComPtr<ID3DBlob> ps;
    if (vs) ps = ShaderCompiler::compile(file, "PSMain", "ps_5_0", log);
    if (!vs || !ps) {
        error = log;
        return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = rootSignature;
    desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    if (options.inputLayout) desc.InputLayout = { options.inputLayout, options.inputCount };

    // ブレンドしない (上書き)。 BlendEnable は FALSE だが、 列挙の値は 0 だと不正なので正しい値を入れる
    D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.SampleMask = UINT_MAX;

    desc.RasterizerState.FillMode = options.fill;
    desc.RasterizerState.CullMode = options.cull;
    desc.RasterizerState.FrontCounterClockwise = TRUE;  // 外から見て反時計回りが表
    desc.RasterizerState.DepthClipEnable = TRUE;

    const D3D12_DEPTH_STENCILOP_DESC keep = {
        D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    desc.DepthStencilState.DepthEnable = options.depthTest ? TRUE : FALSE;
    desc.DepthStencilState.DepthWriteMask =
        options.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    desc.DepthStencilState.FrontFace = keep;
    desc.DepthStencilState.BackFace = keep;

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = setup.colorFormat;
    desc.DSVFormat = setup.depthFormat;
    desc.SampleDesc.Count = 1;

    ComPtr<ID3D12PipelineState> created;
    const HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&created));
    if (FAILED(hr)) {
        char text[96];
        std::snprintf(text, sizeof(text), "CreateGraphicsPipelineState failed (HRESULT 0x%08X)",
                      static_cast<unsigned>(hr));
        error = text;
        return false;
    }
    pipeline = created;  // 成功したときだけ差し替える
    return true;
}

} // namespace gpu
