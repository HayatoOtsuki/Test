#include "Window.h"

#include "DxCheck.h"

#include <windowsx.h>  // GET_X_LPARAM / GET_Y_LPARAM

namespace {

constexpr wchar_t kClassName[] = L"SdfIceWindow";

} // namespace

Window::Window(HINSTANCE instance, const wchar_t* title, UINT width, UINT height)
    : mInstance(instance), mWidth(width), mHeight(height)
{
    // 1. ウィンドウクラスを登録する。 lpfnWndProc がメッセージの届け先
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &Window::windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc)) ThrowLastError("RegisterClassEx");

    // 2. 内側 (クライアント領域) が width × height になるよう、 枠とタイトルバーの分を足した大きさを求める
    RECT rect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    // 3. 作る。 最後の引数の this は WM_NCCREATE で受け取り、 windowProc から自分を見つけるのに使う
    mHwnd = CreateWindowExW(0, kClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, this);
    if (!mHwnd) ThrowLastError("CreateWindowEx");
}

Window::~Window()
{
    if (mHwnd) DestroyWindow(mHwnd);
    UnregisterClassW(kClassName, mInstance);
}

void Window::show(int showCommand)
{
    ShowWindow(mHwnd, showCommand);
}

void Window::setTitle(const wchar_t* title)
{
    if (mHwnd) SetWindowTextW(mHwnd, title);
}

void Window::close()
{
    // WM_DESTROY が届き、 そこで終了 (WM_QUIT) を出す
    if (mHwnd) DestroyWindow(mHwnd);
}

bool Window::processMessages()
{
    // PeekMessage はメッセージが無くても待たずに戻る。 空になったら描画へ進む
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);  // ここから windowProc が呼ばれる
    }
    return true;
}

LRESULT CALLBACK Window::windowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // windowProc は static なので、 どの Window の物か分からない。
    // そこで最初の頃に届く WM_NCCREATE で、 CreateWindowEx に渡した this をウィンドウ (GWLP_USERDATA) にしまう
    if (msg == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lp);
        auto* window = static_cast<Window*>(create->lpCreateParams);
        window->mHwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }
    // しまう前 (WM_NCCREATE より前) と片付けた後のメッセージは、 既定の処理に任せる
    auto* window = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return window ? window->handleMessage(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT Window::handleMessage(UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_LBUTTONDOWN:
        // ドラッグの始まり。 キャプチャすると、 マウスがウィンドウの外へ出ても動きが届く
        mDragging = true;
        mLastMouse = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        SetCapture(mHwnd);
        if (mCallbacks.onDragBegin) mCallbacks.onDragBegin();
        return 0;

    case WM_LBUTTONUP:
        mDragging = false;
        ReleaseCapture();
        return 0;

    case WM_CAPTURECHANGED:
        // ほかにキャプチャを取られた (Alt+Tab など) ときもドラッグをやめる
        mDragging = false;
        return 0;

    case WM_MOUSEMOVE:
        if (mDragging) {
            // 前の位置との差を知らせる
            const POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            if (mCallbacks.onDrag) mCallbacks.onDrag(p.x - mLastMouse.x, p.y - mLastMouse.y);
            mLastMouse = p;
        }
        return 0;

    case WM_MOUSEWHEEL:
        // 1 目盛りは WHEEL_DELTA (120)。 細かく回せるホイールのため、 割った値を小数で渡す
        if (mCallbacks.onWheel) {
            mCallbacks.onWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / WHEEL_DELTA);
        }
        return 0;

    case WM_KEYDOWN:
        if (mCallbacks.onKeyDown) mCallbacks.onKeyDown(wp);
        return 0;

    case WM_SIZE:
        // 大きさを覚えるだけ。 バックバッファは Application が次のフレームの前に合わせる。
        // 最小化 (0 × 0) は無視する。 バックバッファを 0 にはできない
        if (wp != SIZE_MINIMIZED && LOWORD(lp) > 0 && HIWORD(lp) > 0) {
            mWidth = LOWORD(lp);
            mHeight = HIWORD(lp);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);  // メッセージループに終わりを知らせる
        return 0;

    case WM_NCDESTROY: {
        // ウィンドウが無くなる最後のメッセージ。 しまった this を外し、 もう使わないようにする
        const HWND hwnd = mHwnd;
        mHwnd = nullptr;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    }
    return DefWindowProcW(mHwnd, msg, wp, lp);
}
