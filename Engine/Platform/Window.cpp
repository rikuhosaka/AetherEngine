#include "Window.h"

//---------------------------------------------
// �E�B���h�E�v���V�[�W��
//---------------------------------------------
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY: // �~�{�^���������ꂽ��
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

//---------------------------------------------
// �E�B���h�E�쐬�֐�
//---------------------------------------------
HWND CreateGameWindow(HINSTANCE hInstance)
{
    
    // �E�B���h�E�N���X����������
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW; // �ĕ`��X�^�C��
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = _T("DirectXEngine");

    // �N���X�o�^
    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, _T("ウィンドウクラスの登録に失敗しました"), _T("エラー"), MB_OK);
        return nullptr;
    }

    // �E�B���h�E�T�C�Y��␳
    RECT wrc = { 0, 0, (LONG)g_windowWidth, (LONG)g_windowHeight };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

    //window生成
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
    UnregisterClass(wc.lpszClassName, wc.hInstance);
}