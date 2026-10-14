#pragma once
// =============================================================================
// ShaderCompiler.h — HLSL のファイルをコンパイルする
//
// 実行時に D3DCompileFromFile (シェーダーモデル 5.0) でコンパイルする。
// ファイルを書き換えて R キーを押せば、 起動したまま読み直せる。
// =============================================================================

#include <d3dcommon.h>
#include <wrl/client.h>

#include <string>

class ShaderCompiler {
public:
    // path のファイルを、 入口の関数 entry についてコンパイルする。
    //   target: シェーダーの種類と版 (vs_5_0 = 頂点シェーダー 5.0、 ps_5_0 = ピクセルシェーダー 5.0)
    // 失敗したら nullptr を返す。 エラーと警告の文は log の後ろに足す。
    // #include "x.hlsli" は、 path のファイルがある場所から探す。
    static Microsoft::WRL::ComPtr<ID3DBlob> compile(const std::wstring& path, const char* entry, const char* target,
                                                    std::string& log);
};
