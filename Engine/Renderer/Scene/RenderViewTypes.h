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

struct alignas(256) FrameConstants
{
	float viewMatrix[16]{};
	float projectionMatrix[16]{};
	float viewProjectionMatrix[16]{};
	float cameraPosition[4]{};
	float ambientColor[4]{};
	float mainLightDirection[4]{};
	float mainLightColor[4]{};
};

static_assert(sizeof(FrameConstants) == 256);

struct alignas(256) ObjectConstants
{
	float worldMatrix[16]{};
	float worldInverseTranspose[16]{};
	float padding[32]{};
};

static_assert(sizeof(ObjectConstants) == 256);
