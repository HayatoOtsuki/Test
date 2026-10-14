#include "Application.h"

#include <chrono>
#include <cmath>
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
const wchar_t* const kShapeNames[] = { L"Cube", L"Sphere", L"Cylinder", L"SDF" };
static_assert(sizeof(kShapeNames) / sizeof(kShapeNames[0]) == static_cast<size_t>(IceShape::Count),
              "kShapeNames must have one name per IceShape");

constexpr float kMaterialStep = 0.1f;   // [ ] , . で値を変える幅
constexpr float kSunTurnStep = 0.26f;   // J / L で太陽を回す角度 (ラジアン、 約 15°)

// 向き d を y 軸の周りに angle だけ回す (太陽の高さは変えずに、 方角だけ変える)
Float3 rotateY(Float3 d, float angle)
{
    const float c = std::cos(angle), s = std::sin(angle);
    return { c * d.x + s * d.z, d.y, -s * d.x + c * d.z };
}

} // namespace

Application::Application(HINSTANCE instance)
    : mWindow(instance, L"SDF Ice", kInitialWidth, kInitialHeight)
    , mRenderer(mWindow.getHandle(), mWindow.getWidth(), mWindow.getHeight(), L"" SHADER_ROOT L"/engine/shaders")
    , mIce(L"" SHADER_ROOT L"/ice/shaders")
{
    // ウィンドウの入力をカメラとキーの処理へつなぐ
    Window::Callbacks callbacks;
    callbacks.onDragBegin = [this] { mCamera.stopAutoRotate(); };
    callbacks.onDrag = [this](int dx, int dy) { mCamera.rotate(dx, dy); };
    callbacks.onWheel = [this](float notches) { mCamera.zoom(notches); };
    callbacks.onKeyDown = [this](WPARAM key) { onKeyDown(key); };
    mWindow.setCallbacks(std::move(callbacks));

    // 氷を床の真ん中に置き、 Renderer に登録する (ここで氷のパイプラインが作られ、 以後毎フレーム描かれる)。
    // 位置のセッターは IceComponent が継承した RenderEntity が持つ
    mIce.setPosition({ 0.0f, 0.0f, 0.0f });
    mRenderer.addEntity(&mIce);
    updateTitle();
}

Application::~Application()
{
    // mIce が壊れる前に、 Renderer の一覧から外す
    mRenderer.removeEntity(&mIce);
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
    // 太陽を回す: J で左回り、 L で右回り。 値は毎フレーム シーンの定数へ書かれ、 背景とプリミティブの両方に届く
    case 'J': mSun.direction = rotateY(mSun.direction, kSunTurnStep); break;
    case 'L': mSun.direction = rotateY(mSun.direction, -kSunTurnStep); break;
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
    // 背景と、 登録された物 (氷) のシェーダーをまとめて読み直す
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

        mRenderer.renderFrame(mCamera, mSun, time);  // 背景と氷を描いて表示する
    }
    return 0;
}
