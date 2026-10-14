#pragma once
// =============================================================================
// Application.h — 部品をつなぐ
//
//   Window      : 入力を受け取る → カメラへ渡す
//   OrbitCamera : 角度と距離を持つ
//   Renderer    : フレームの始まり (背景) と終わり (表示) を受け持つ
//   IceComponent: 氷の部品。 beginFrame と endFrame の間に draw3D を呼ぶ
// =============================================================================

#include "OrbitCamera.h"
#include "Renderer.h"
#include "Window.h"

#include "ice/IceComponent.h"

class Application {
public:
    explicit Application(HINSTANCE instance);

    // ウィンドウを出してメッセージループを回す。 ウィンドウが閉じたら戻る
    int run(int showCommand);

private:
    void onKeyDown(WPARAM key);
    void reloadShaders();
    void setShape(IceShape shape);  // 形を変え、 ウィンドウの題名に形の名前を出す

    // 宣言の順に作られる。 Renderer はウィンドウのハンドルを使うので Window の後
    Window mWindow;
    OrbitCamera mCamera;
    Renderer mRenderer;
    IceComponent mIce;  // Renderer の後に作り、 先に壊す (GPU のリソースを持つため)
    bool mReloadRequested = false;  // R キーが押された。 次のフレームの前にシェーダーを読み直す
};
