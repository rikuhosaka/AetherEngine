#include "InputManager.h"
#include <Engine/Platform/Window.h>

void InputManager::Init() {
	std::memset(keys, 0, sizeof(keys));
	std::memset(lastKeys, 0, sizeof(lastKeys));
	std::memset(keyState, 0, sizeof(keyState));

	// マウス更新
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(g_hwnd, &p);
	lastMousePos = mousePos;
	mousePos.x = static_cast<float>(p.x);
	mousePos.y = static_cast<float>(p.y);

	mousePos.x = (2.0f * mousePos.x / g_windowWidth) - 1.0f;
	mousePos.y = (2.0f * mousePos.y / g_windowHeight) - 1.0f;
}

void
InputManager::Update() {
	std::memcpy(lastKeys, keys, sizeof(keys));

	if (GetKeyboardState(keyState)) {
		for (int i = 0; i < 256; ++i)
			keys[i] = (keyState[i] & 0x80) != 0;
	}

	// マウス更新
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(g_hwnd, &p);
	lastMousePos = mousePos;
	mousePos.x = static_cast<float>(p.x);
	mousePos.y = static_cast<float>(p.y);

	mousePos.x = (2.0f * mousePos.x / g_windowWidth) - 1.0f;
	mousePos.y = (2.0f * mousePos.y / g_windowHeight) - 1.0f;
}

