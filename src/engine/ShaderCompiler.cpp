#include "ShaderCompiler.h"

#include <d3dcompiler.h>

#include <cstdio>

using Microsoft::WRL::ComPtr;

ComPtr<ID3DBlob> ShaderCompiler::compile(const std::wstring& path, const char* entry, const char* target,
                                         std::string& log)
{
    // ENABLE_STRICTNESS  : 古い書き方を許さない
    // OPTIMIZATION_LEVEL3: いちばん強い最適化 (光線を追うループが速くなる)
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG;  // PIX などのツールでシェーダーのソースを追えるようにする
#endif

    // D3D_COMPILE_STANDARD_FILE_INCLUDE: #include を、 コンパイルするファイルのある場所から探す
    ComPtr<ID3DBlob> code, messages;
    const HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, target,
                                          flags, 0, &code, &messages);
    if (messages) {
        log += static_cast<const char*>(messages->GetBufferPointer());
        OutputDebugStringA(static_cast<const char*>(messages->GetBufferPointer()));  // 警告も出力ウィンドウへ
    }
    if (FAILED(hr)) {
        // ファイルが見つからないときは messages が空なので、 HRESULT だけ書いておく
        if (!messages) {
            char text[96];
            std::snprintf(text, sizeof(text), "D3DCompileFromFile(%s) failed (HRESULT 0x%08X)\n", entry,
                          static_cast<unsigned>(hr));
            log += text;
        }
        return nullptr;
    }
    return code;
}
