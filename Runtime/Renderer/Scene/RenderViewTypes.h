#pragma once

#include "Runtime/Engine/Camera/ExtractedView.h"

#include <cstdint>

struct alignas(256) FrameConstants
{
	float viewMatrix[16]{};
	float projectionMatrix[16]{};
	float viewProjectionMatrix[16]{};
	float cameraPosition[4]{};
	float ambientColor[4]{};
	float mainLightDirection[4]{};
	float mainLightColor[4]{};
	float lightViewProjection[16]{};
	float shadowParams[4]{};
	float shadowPadding[44]{};
};

static_assert(sizeof(FrameConstants) == 512);

struct alignas(256) ObjectConstants
{
	float worldMatrix[16]{};
	float worldInverseTranspose[16]{};
	float padding[32]{};
};

static_assert(sizeof(ObjectConstants) == 256);
