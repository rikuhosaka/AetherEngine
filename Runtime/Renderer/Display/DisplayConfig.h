#pragma once

#include <cstdint>

struct DisplayConfig
{
	uint32_t width = 1280;
	uint32_t height = 720;
	uint32_t bufferCount = 3;
	float clearColor[4]{ 0.1f, 0.1f, 0.15f, 1.0f };
	float clearDepth = 1.0f;
	uint8_t clearStencil = 0;
};
