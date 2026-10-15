#pragma once
// =============================================================================
// SampleApp.h — 簡易サンプルを 1 つ回す
//
// 窓・カメラ・太陽・Renderer を持ち、 渡されたサンプル (SampleEntity) を登録して、 窓が閉じるまで毎フレーム描く。
// どのサンプルでも同じキー:
//   左ドラッグ: カメラを回す / ホイール: 寄る・引く / Space: 自動回転 / J・L: 太陽を回す / R: シェーダーを読み直す / Esc: 終わる
// それ以外のキーはサンプルの onKey へ渡す。
// =============================================================================

#include "common/SampleEntity.h"

#include "engine/DirectionalLight.h"
#include "engine/OrbitCamera.h"
#include "engine/Renderer.h"
#include "engine/Window.h"

#include <string>

class SampleApp {
public:
    // title: ウィンドウの題名の頭 (例: L"01 プリミティブだけ")
    SampleApp(HINSTANCE instance, std::wstring title);

    SampleApp(const SampleApp&) = delete;
    SampleApp& operator=(const SampleApp&) = delete;

    // sample を Renderer に登録して、 窓が閉じるまで回す。 終わるときに登録を外す
    int run(SampleEntity& sample, int showCommand);

private:
    void onKeyDown(WPARAM key);
    void updateTitle();

    std::wstring mTitle;
    Window mWindow;
    OrbitCamera mCamera;
    DirectionalLight mSun;
    Renderer mRenderer;
    SampleEntity* mSample = nullptr;  // run の間だけ指す
    bool mReloadRequested = false;
};
