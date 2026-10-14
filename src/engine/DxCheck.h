#pragma once
// =============================================================================
// DxCheck.h — 失敗を例外にする小さな道具
//
// DirectX と Win32 の関数の多くは、 成功か失敗かを HRESULT (32 bit の値) で返す。
// 毎回 if で調べると読みにくいので、 失敗したら例外を投げる関数にまとめる。
// 例外は main.cpp でまとめて受け取り、 メッセージボックスで表示する。
// =============================================================================

#include <windows.h>

#include <cstdio>
#include <stdexcept>

// hr が失敗 (負の値) なら、 呼び出しの名前と HRESULT の値を入れた例外を投げる。
//   what: 失敗した呼び出しの名前 (例: "CreateCommandQueue")
inline void ThrowIfFailed(HRESULT hr, const char* what)
{
    if (SUCCEEDED(hr)) return;
    char message[256];
    std::snprintf(message, sizeof(message), "%s failed (HRESULT 0x%08X)", what, static_cast<unsigned>(hr));
    throw std::runtime_error(message);
}

// HRESULT ではなく GetLastError で失敗を知らせる Win32 の関数 (CreateWindowEx など) 用。
// 直前のエラーの番号を HRESULT に直して例外を投げる。
inline void ThrowLastError(const char* what)
{
    const DWORD code = GetLastError();
    ThrowIfFailed(code != 0 ? HRESULT_FROM_WIN32(code) : E_FAIL, what);
}
