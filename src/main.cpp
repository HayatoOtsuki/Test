// =============================================================================
// main.cpp — 入口
//
// Application を作って回すだけ。
// 途中で起きた失敗 (ThrowIfFailed の例外) はここで受け取り、 メッセージボックスで見せる。
// =============================================================================

#include "engine/Application.h"

#include <exception>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand)
{
    try {
        Application app(instance);
        return app.run(showCommand);
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "SDF Ice", MB_ICONERROR);
        return 1;
    }
}
