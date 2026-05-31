#pragma once

#include <Windows.h>

#include <cstdint>

struct WindowServices
{
	HWND hwnd = nullptr;
	HINSTANCE hInstance = nullptr;
	uint32_t clientWidth = 0;
	uint32_t clientHeight = 0;
	bool isMinimized = false;
};
