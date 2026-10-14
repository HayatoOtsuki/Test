#pragma once
// =============================================================================
// Application.h — 部品をつなぐ
//
//   Window      : 入力を受け取る → カメラへ渡す
//   OrbitCamera : 角度と距離を持つ
//   Renderer    : 1 フレームの描画 (背景と、 登録された物)
//   IceComponent: 氷の部品。 作ると Renderer に描画が登録され、 以後 Renderer が毎フレーム描く
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
    void setShape(IceShape shape);  // 形を変える
    void updateTitle();             // ウィンドウの題名に、 今の形とマテリアルの値を出す

    // 宣言の順に作られる。 Renderer はウィンドウのハンドルを使うので Window の後
    Window mWindow;
    OrbitCamera mCamera;
    Renderer mRenderer;
    IceComponent mIce;  // Renderer の後に作り、 先に壊す (壊れるときに Renderer から登録を外すため)
    bool mReloadRequested = false;  // R キーが押された。 次のフレームの前にシェーダーを読み直す
};
