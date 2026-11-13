#pragma once

#include <cstdint>

struct ExtractedView
{
	float viewMatrix[16]{};
	float projectionMatrix[16]{};
	float viewProjectionMatrix[16]{};
	float cameraPosition[3]{};
	float pad0 = 0.0f;
	uint32_t viewportWidth = 0;
	uint32_t viewportHeight = 0;
};
