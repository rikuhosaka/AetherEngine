#include "Engine/Platform/Window.h"


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// �E�B���h�E�̍쐬
	g_hwnd = CreateGameWindow(hInstance);
	if (!g_hwnd)
		return -1;

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		MSGProcess(msg);
	}
	return 0;
}