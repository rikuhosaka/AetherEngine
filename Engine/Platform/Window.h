#pragma once
#include <Windows.h>
#include <tchar.h>

//---------------------------------------------
// 定数
//---------------------------------------------
constexpr unsigned int g_windowWidth = 1280;
constexpr unsigned int g_windowHeight = 720;

//---------------------------------------------
// グローバル変数
//---------------------------------------------
inline HWND g_hwnd = nullptr;
inline WNDCLASSEX wc = {};

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

HWND CreateGameWindow(HINSTANCE hInstance);

inline void MSGProcess(MSG& msg)
{
    // メッセージがある場合は処理
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Terminate();
