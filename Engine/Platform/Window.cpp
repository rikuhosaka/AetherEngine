#include "Window.h"

//---------------------------------------------
// ウィンドウプロシージャ
//---------------------------------------------
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY: // ×ボタンが押された時
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

//---------------------------------------------
// ウィンドウ作成関数
//---------------------------------------------
HWND CreateGameWindow(HINSTANCE hInstance)
{
    // ウィンドウクラス情報を初期化
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW; // 再描画スタイル
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = _T("DirectXEngine");

    // クラス登録
    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, _T("ウィンドウクラスの登録に失敗しました"), _T("エラー"), MB_OK);
        return nullptr;
    }

    // ウィンドウサイズを補正
    RECT wrc = { 0, 0, (LONG)g_windowWidth, (LONG)g_windowHeight };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

    // ウィンドウ生成
    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        _T("DX12MyGameEngine"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wrc.right - wrc.left, wrc.bottom - wrc.top,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd)
    {
        MessageBox(NULL, _T("ウィンドウの作成に失敗しました"), _T("エラー"), MB_OK);
        return nullptr;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return hwnd;
}

void Terminate() {
    //もうクラス使わんから登録解除してや
    UnregisterClass(wc.lpszClassName, wc.hInstance);
}