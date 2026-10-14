#include "Application.h"

#include <chrono>
#include <cwchar>
#include <string>
#include <utility>

// シェーダーのフォルダの親 (src)。 CMakeLists.txt が絶対パスを渡す
#ifndef SHADER_ROOT
#define SHADER_ROOT "src"
#endif

namespace {

constexpr UINT kInitialWidth = 1280;
constexpr UINT kInitialHeight = 720;

// ウィンドウの題名に出す形の名前 (IceShape の順)
const wchar_t* const kShapeNames[] = { L"Cube", L"Sphere", L"Cylinder" };
static_assert(sizeof(kShapeNames) / sizeof(kShapeNames[0]) == static_cast<size_t>(IceShape::Count),
              "kShapeNames must have one name per IceShape");

constexpr float kMaterialStep = 0.1f;  // [ ] , . で値を変える幅

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

    // 氷の部品の準備。 描画先の形式と、 氷のシェーダーのフォルダを渡す
    mIce.initialize(mRenderer.getGraphicsDevice().getDevice(), mRenderer.getColorFormat(), mRenderer.getDepthFormat(),
                    L"" SHADER_ROOT L"/ice/shaders");
    setShape(mIce.getShape());
}

void Application::setShape(IceShape shape)
{
    mIce.setShape(shape);
    updateTitle();
}

void Application::updateTitle()
{
    wchar_t title[160];
    std::swprintf(title, sizeof(title) / sizeof(title[0]),
                  L"SDF Ice - %ls | metallic %.1f | roughness %.2f | specular %.2f",
                  kShapeNames[static_cast<int>(mIce.getShape())], mIce.getMetallic(), mIce.getRoughness(),
                  mIce.getSpecular());
    mWindow.setTitle(title);
}

void Application::onKeyDown(WPARAM key)
{
    switch (key) {
    case VK_ESCAPE: mWindow.close(); break;
    case VK_SPACE: mCamera.toggleAutoRotate(); break;
    case 'R': mReloadRequested = true; break;  // メッセージの処理中に GPU を待たないよう、 印だけ付ける
    case VK_TAB: {
        // 次の形へ。 最後の次は最初に戻る
        const int next = (static_cast<int>(mIce.getShape()) + 1) % static_cast<int>(IceShape::Count);
        setShape(static_cast<IceShape>(next));
        break;
    }
    // マテリアルを試す: M で金属かどうか、 [ ] で粗さ、 , . で鏡面反射の強さ
    case 'M':
        mIce.setMetallic(mIce.getMetallic() < 0.5f ? 1.0f : 0.0f);
        updateTitle();
        break;
    case VK_OEM_4:  // [
        mIce.setRoughness(mIce.getRoughness() - kMaterialStep);
        updateTitle();
        break;
    case VK_OEM_6:  // ]
        mIce.setRoughness(mIce.getRoughness() + kMaterialStep);
        updateTitle();
        break;
    case VK_OEM_COMMA:
        mIce.setSpecular(mIce.getSpecular() - kMaterialStep);
        updateTitle();
        break;
    case VK_OEM_PERIOD:
        mIce.setSpecular(mIce.getSpecular() + kMaterialStep);
        updateTitle();
        break;
    default:
        // 1, 2, 3 … で形を選ぶ (IceShape の順)。 範囲の外は setShape が無視する
        if (key >= '1' && key <= '9') setShape(static_cast<IceShape>(key - '1'));
        break;
    }
}

void Application::reloadShaders()
{
    // Renderer::reloadShaders が GPU を待つので、 氷の PSO もその後なら差し替えてよい
    std::string error;
    if (!mRenderer.reloadShaders(error) || !mIce.reloadShaders(error)) {
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

        const DrawContext context = mRenderer.beginFrame(mCamera, time);  // 背景まで描く
        mIce.draw3D(context);
        mRenderer.endFrame();
    }
    return 0;
}
