#pragma once

#include "Engine/Platform/InputState.h"

#include <Windows.h>

#include <cstdint>

struct WindowServices
{
	HWND hwnd = nullptr;
	HINSTANCE hInstance = nullptr;
	uint32_t clientWidth = 0;
	uint32_t clientHeight = 0;
	bool isMinimized = false;
	int mouseWheelDelta = 0;
	TextInput textInput{};
};
