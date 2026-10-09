#pragma once

#include "Engine/Platform/InputState.h"

#include <Windows.h>

#include <array>
#include <cstdint>

class Win32InputDevice
{
public:
	void Reset(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight, InputState& state);
	void Update(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight, InputState& state);

private:
	void WriteMouse(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight, InputState& state);

	std::array<bool, static_cast<std::size_t>(Key::Count)> m_wasDown{};
	MousePixelPosition m_lastMousePixelPos{};
};
