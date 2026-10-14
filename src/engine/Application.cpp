#include "Application.h"

#include <chrono>
#include <string>
#include <utility>

// シェーダーのフォルダの親 (src)。 CMakeLists.txt が絶対パスを渡す
#ifndef SHADER_ROOT
#define SHADER_ROOT "src"
#endif

namespace {

constexpr UINT kInitialWidth = 1280;
constexpr UINT kInitialHeight = 720;

} // namespace

Application::Application(HINSTANCE instance)
    : mWindow(instance, L"SDF Ice", kInitialWidth, kInitialHeight)
    , mRenderer(mWindow.getHandle(), mWindow.getWidth(), mWindow.getHeight(), L"" SHADER_ROOT L"/engine/shaders")
{
    // ウィンドウの入力をカメラとキーの処理へつなぐ
    Window::Callbacks callbacks;
    callbacks.onDragBegin = [this] { mCamera.stopAutoRotate(); };
    callbacks.onDrag = [this](int dx, int dy) { mCamera.rotate(dx, dy); };
    callbacks.onWheel = [this](float notches) { mCamera.zoom(notches); };
    callbacks.onKeyDown = [this](WPARAM key) { onKeyDown(key); };
    mWindow.setCallbacks(std::move(callbacks));
}

void Application::onKeyDown(WPARAM key)
{
    switch (key) {
    case VK_ESCAPE: mWindow.close(); break;
    case VK_SPACE: mCamera.toggleAutoRotate(); break;
    case 'R': mReloadRequested = true; break;  // メッセージの処理中に GPU を待たないよう、 印だけ付ける
    default: break;
    }
}

void Application::reloadShaders()
{
    std::string error;
    if (!mRenderer.reloadShaders(error)) {
        MessageBoxA(mWindow.getHandle(), error.c_str(), "Shader error", MB_ICONERROR);
    }
}

int Application::run(int showCommand)
{
    mWindow.show(showCommand);

    using Clock = std::chrono::steady_clock;
    const Clock::time_point start = Clock::now();
    Clock::time_point previous = start;

    // 1 周 = 1 フレーム: 入力を処理 → 状態を進める → 描く
    while (mWindow.processMessages()) {
        const Clock::time_point now = Clock::now();
        const float dt = std::chrono::duration<float>(now - previous).count();
        const float time = std::chrono::duration<float>(now - start).count();
        previous = now;

        if (mReloadRequested) {
            mReloadRequested = false;
            reloadShaders();
        }

        // ウィンドウの大きさが変わっていたら、 描く前にバックバッファを合わせる
        mRenderer.resize(mWindow.getWidth(), mWindow.getHeight());
        mCamera.update(dt);

        mRenderer.beginFrame(mCamera, time);  // 背景まで描く
        mRenderer.endFrame();
    }
    return 0;
}
