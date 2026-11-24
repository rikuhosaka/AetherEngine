#pragma once

#include "Runtime/Engine/Camera/CameraState.h"
#include "Runtime/Renderer/Scene/ExtractedView.h"

#include <cstdint>

void BuildExtractedView(
	const CameraState& camera,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	ExtractedView& outView);
