#include "InputManager.h"

void InputManager::Initialize(HWND hwnd, uint32_t clientWidth, uint32_t clientHeight)
{
	std::memset(keys, 0, sizeof(keys));
	std::memset(lastKeys, 0, sizeof(lastKeys));
	std::memset(keyState, 0, sizeof(keyState));
	UpdateMousePosition(hwnd, clientWidth, clientHeight);
}

void InputManager::Update(HWND hwnd, uint32_t clientWidth, uint32_t clientHeight)
{
	std::memcpy(lastKeys, keys, sizeof(keys));

	if (GetKeyboardState(keyState))
	{
		for (int i = 0; i < 256; ++i)
		{
			keys[i] = (keyState[i] & 0x80) != 0;
		}
	}

	UpdateMousePosition(hwnd, clientWidth, clientHeight);
}

void InputManager::UpdateMousePosition(HWND hwnd, uint32_t clientWidth, uint32_t clientHeight)
{
	if (hwnd == nullptr || clientWidth == 0 || clientHeight == 0)
	{
		return;
	}

	POINT point{};
	GetCursorPos(&point);
	ScreenToClient(hwnd, &point);

	lastMousePos = mousePos;
	mousePos.x = static_cast<float>(point.x);
	mousePos.y = static_cast<float>(point.y);

	mousePos.x = (2.0f * mousePos.x / static_cast<float>(clientWidth)) - 1.0f;
	mousePos.y = (2.0f * mousePos.y / static_cast<float>(clientHeight)) - 1.0f;
}
