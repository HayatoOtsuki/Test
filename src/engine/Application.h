#pragma once
// =============================================================================
// Application.h — 部品をつなぐ
//
//   Window      : 入力を受け取る → カメラへ渡す
//   OrbitCamera : 角度と距離を持つ
//   Renderer    : フレームの始まり (背景) と終わり (表示) を受け持つ
// =============================================================================

#include "OrbitCamera.h"
#include "Renderer.h"
#include "Window.h"

class Application {
public:
    explicit Application(HINSTANCE instance);

    // ウィンドウを出してメッセージループを回す。 ウィンドウが閉じたら戻る
    int run(int showCommand);

private:
    void onKeyDown(WPARAM key);
    void reloadShaders();

    // 宣言の順に作られる。 Renderer はウィンドウのハンドルを使うので Window の後
    Window mWindow;
    OrbitCamera mCamera;
    Renderer mRenderer;
    bool mReloadRequested = false;  // R キーが押された。 次のフレームの前にシェーダーを読み直す
};
