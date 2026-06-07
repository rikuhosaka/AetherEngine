#include "InputManager.h"

void InputManager::Initialize(HWND hwnd, uint32_t clientWidth, uint32_t clientHeight)
{
	std::memset(keys, 0, sizeof(keys));
	std::memset(lastKeys, 0, sizeof(lastKeys));
	std::memset(keyState, 0, sizeof(keyState));
	std::memset(mouseButtons, 0, sizeof(mouseButtons));
	std::memset(lastMouseButtons, 0, sizeof(lastMouseButtons));
	mousePixelPos = {};
	lastMousePixelPos = {};
	UpdateMousePosition(hwnd, clientWidth, clientHeight);
	lastMousePixelPos = mousePixelPos;
}

void InputManager::Update(HWND hwnd, uint32_t clientWidth, uint32_t clientHeight)
{
	std::memcpy(lastKeys, keys, sizeof(keys));
	std::memcpy(lastMouseButtons, mouseButtons, sizeof(mouseButtons));

	if (GetKeyboardState(keyState))
	{
		for (int i = 0; i < 256; ++i)
		{
			keys[i] = (keyState[i] & 0x80) != 0;
		}
	}

	UpdateMouseButtons();
	UpdateMousePosition(hwnd, clientWidth, clientHeight);
}

void InputManager::UpdateMouseButtons()
{
	static constexpr int kTrackedMouseButtons[] = {
		VK_LBUTTON,
		VK_RBUTTON,
		VK_MBUTTON,
		VK_XBUTTON1,
		VK_XBUTTON2,
	};

	for (const int button : kTrackedMouseButtons)
	{
		mouseButtons[button] = (GetAsyncKeyState(button) & 0x8000) != 0;
	}
}

void InputManager::UpdateMousePosition(HWND hwnd, uint32_t clientWidth, uint32_t clientHeight)
{
	lastMousePos = mousePos;
	lastMousePixelPos = mousePixelPos;

	if (hwnd == nullptr || clientWidth == 0 || clientHeight == 0)
	{
		return;
	}

	POINT point{};
	GetCursorPos(&point);
	ScreenToClient(hwnd, &point);

	mousePixelPos.x = point.x;
	mousePixelPos.y = point.y;

	mousePos.x = (2.0f * static_cast<float>(point.x) / static_cast<float>(clientWidth)) - 1.0f;
	mousePos.y = (2.0f * static_cast<float>(point.y) / static_cast<float>(clientHeight)) - 1.0f;
}
