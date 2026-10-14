#pragma once
// =============================================================================
// Window.h — Win32 のウィンドウ 1 枚
//
// メッセージ (マウス・キー・大きさの変化など) を受け取り、 入力を登録された関数で知らせる。
// カメラや描画のことは知らない。 つなぐのは Application の役目。
// =============================================================================

#include <windows.h>

#include <functional>
#include <utility>

class Window {
public:
    // 知らせる入力。 使わないものは空のままでよい
    struct Callbacks {
        std::function<void()> onDragBegin;           // 左ボタンを押した
        std::function<void(int dx, int dy)> onDrag;  // 左ボタンを押したままマウスが動いた (画素)
        std::function<void(float notches)> onWheel;  // ホイールを回した (1 目盛り = 1.0、 奥へ回すと正)
        std::function<void(WPARAM key)> onKeyDown;   // キーを押した (仮想キーコード)
    };

    // 内側 (描ける所) が width × height 画素のウィンドウを作る。 まだ表示はしない
    Window(HINSTANCE instance, const wchar_t* title, UINT width, UINT height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void setCallbacks(Callbacks callbacks) { mCallbacks = std::move(callbacks); }
    void show(int showCommand);
    void close();

    // たまっているメッセージを全部処理する。 終了 (WM_QUIT) が来たら false を返す
    bool processMessages();

    HWND getHandle() const { return mHwnd; }
    // 内側の今の大きさ (最小化中は最小化する前の大きさ)
    UINT getWidth() const { return mWidth; }
    UINT getHeight() const { return mHeight; }

private:
    // Win32 から呼ばれる入口 (static)。 ウィンドウに覚えさせた this を取り出して handleMessage へ渡す
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handleMessage(UINT msg, WPARAM wp, LPARAM lp);

    HINSTANCE mInstance;
    HWND mHwnd = nullptr;
    UINT mWidth;
    UINT mHeight;
    Callbacks mCallbacks;
    bool mDragging = false;
    POINT mLastMouse = {};
};
