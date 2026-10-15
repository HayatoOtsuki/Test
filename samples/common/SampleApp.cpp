#include "common/SampleApp.h"

#include <chrono>
#include <cmath>
#include <utility>

// エンジンのシェーダーのフォルダ (背景の空と床)。 samples/CMakeLists.txt が絶対パスを渡す
#ifndef ENGINE_SHADER_DIR
#define ENGINE_SHADER_DIR "src/engine/shaders"
#endif

namespace {

constexpr UINT kInitialWidth = 1280;
constexpr UINT kInitialHeight = 720;
constexpr float kSunTurnStep = 0.26f;  // J / L で太陽を回す角度 (約 15°)

// 向き d を y 軸の周りに angle だけ回す
Float3 rotateY(Float3 d, float angle)
{
    const float c = std::cos(angle), s = std::sin(angle);
    return { c * d.x + s * d.z, d.y, -s * d.x + c * d.z };
}

} // namespace

SampleApp::SampleApp(HINSTANCE instance, std::wstring title)
    : mTitle(std::move(title))
    , mWindow(instance, mTitle.c_str(), kInitialWidth, kInitialHeight)
    , mRenderer(mWindow.getHandle(), mWindow.getWidth(), mWindow.getHeight(), L"" ENGINE_SHADER_DIR)
{
    Window::Callbacks callbacks;
    callbacks.onDragBegin = [this] { mCamera.stopAutoRotate(); };
    callbacks.onDrag = [this](int dx, int dy) { mCamera.rotate(dx, dy); };
    callbacks.onWheel = [this](float notches) { mCamera.zoom(notches); };
    callbacks.onKeyDown = [this](WPARAM key) { onKeyDown(key); };
    mWindow.setCallbacks(std::move(callbacks));
}

void SampleApp::onKeyDown(WPARAM key)
{
    switch (key) {
    case VK_ESCAPE: mWindow.close(); break;
    case VK_SPACE: mCamera.toggleAutoRotate(); break;
    case 'R': mReloadRequested = true; break;  // メッセージの処理中に GPU を待たないよう、 印だけ付ける
    case 'J': mSun.direction = rotateY(mSun.direction, kSunTurnStep); break;
    case 'L': mSun.direction = rotateY(mSun.direction, -kSunTurnStep); break;
    default:
        if (mSample) mSample->onKey(key);  // 残りのキーはサンプルが使う
        break;
    }
    updateTitle();
}

void SampleApp::updateTitle()
{
    std::wstring title = mTitle;
    const std::wstring status = mSample ? mSample->getStatus() : L"";
    if (!status.empty()) title += L" | " + status;
    mWindow.setTitle(title.c_str());
}

int SampleApp::run(SampleEntity& sample, int showCommand)
{
    // 登録すると、 Renderer がサンプルの initializeImplement を呼ぶ (パイプラインなどを作る)
    mSample = &sample;
    mRenderer.addEntity(&sample);
    updateTitle();
    mWindow.show(showCommand);

    using Clock = std::chrono::steady_clock;
    const Clock::time_point start = Clock::now();
    Clock::time_point previous = start;

    while (mWindow.processMessages()) {
        const Clock::time_point now = Clock::now();
        const float dt = std::chrono::duration<float>(now - previous).count();
        const float time = std::chrono::duration<float>(now - start).count();
        previous = now;

        if (mReloadRequested) {
            mReloadRequested = false;
            std::string error;
            if (!mRenderer.reloadShaders(error)) {
                MessageBoxA(mWindow.getHandle(), error.c_str(), "Shader error", MB_ICONERROR);
            }
        }

        mRenderer.resize(mWindow.getWidth(), mWindow.getHeight());
        mCamera.update(dt);
        mRenderer.renderFrame(mCamera, mSun, time);  // 背景 (空と床) → サンプルの drawImplement
    }

    // サンプルはこの後で壊れるので、 先に一覧から外す
    mRenderer.removeEntity(&sample);
    mSample = nullptr;
    return 0;
}
