#pragma once

#include <Windows.h>

#include <cstdint>
#include <cstring>

struct MousePosition
{
	float x;
	float y;
};

struct MousePixelPosition
{
	int x = 0;
	int y = 0;
};

class InputManager
{
public:
	static InputManager& Get()
	{
		static InputManager instance;
		return instance;
	}

	void Initialize(HWND hwnd, uint32_t clientWidth, uint32_t clientHeight);
	void Update(HWND hwnd, uint32_t clientWidth, uint32_t clientHeight);

	bool IsKeyPressed(int key) const { return keys[key]; }
	bool IsKeyDown(int key) const { return keys[key] && !lastKeys[key]; }
	bool IsKeyUp(int key) const { return !keys[key] && lastKeys[key]; }

	bool IsMouseButtonDown(int button) const { return mouseButtons[button]; }
	bool IsMouseButtonPressed(int button) const { return mouseButtons[button] && !lastMouseButtons[button]; }
	bool IsMouseButtonUp(int button) const { return !mouseButtons[button] && lastMouseButtons[button]; }

	MousePosition GetMousePositionNDC() const { return mousePos; }
	MousePosition GetMousePositionDelta() const
	{
		return { mousePos.x - lastMousePos.x, mousePos.y - lastMousePos.y };
	}

	[[nodiscard]] MousePixelPosition GetMousePositionPixels() const { return mousePixelPos; }
	[[nodiscard]] MousePixelPosition GetMouseDeltaPixels() const
	{
		return {
			mousePixelPos.x - lastMousePixelPos.x,
			mousePixelPos.y - lastMousePixelPos.y,
		};
	}

private:
	InputManager() = default;
	InputManager(const InputManager&) = delete;
	void operator=(const InputManager&) = delete;
	~InputManager() = default;

	void UpdateMouseButtons();
	void UpdateMousePosition(HWND hwnd, uint32_t clientWidth, uint32_t clientHeight);

	bool keys[256] = {};
	bool lastKeys[256] = {};
	byte keyState[256] = {};

	bool mouseButtons[256] = {};
	bool lastMouseButtons[256] = {};

	MousePosition mousePos{};
	MousePosition lastMousePos{};
	MousePixelPosition mousePixelPos{};
	MousePixelPosition lastMousePixelPos{};
};
