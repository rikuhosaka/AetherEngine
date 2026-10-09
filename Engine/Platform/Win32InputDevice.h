#pragma once

#include "Engine/Platform/InputState.h"

#include <Windows.h>

#include <array>
#include <cstdint>

class Win32InputDevice
{
public:
	Win32InputDevice() = default;
	~Win32InputDevice();

	Win32InputDevice(const Win32InputDevice&) = delete;
	Win32InputDevice& operator=(const Win32InputDevice&) = delete;

	void Reset(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight, InputState& state);
	void Update(
		HWND hwnd,
		std::uint32_t clientWidth,
		std::uint32_t clientHeight,
		InputState& state,
		bool relativeMouseRequested);
	void RefreshRelativeMouse(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight, InputState& state);

private:
	void WriteButtons(bool focused, InputState& state);
	void WriteMouse(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight, InputState& state);
	void BeginRelativeMouse(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight, InputState& state);
	void EndRelativeMouse(HWND hwnd, InputState& state);
	void ApplyRelativeMouse(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight, InputState& state);
	void HideCursor();
	void RestoreCursor();

	std::array<bool, static_cast<std::size_t>(Key::Count)> m_wasDown{};
	MousePixelPosition m_lastMousePixelPos{};
	POINT m_savedCursorScreen{};
	bool m_relativeActive = false;
	bool m_cursorHidden = false;
};
