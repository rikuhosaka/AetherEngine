#pragma once

#include "Engine/Camera/CameraState.h"

#include <cstdint>

struct ExtractedView;

void BuildExtractedView(
	const CameraState& camera,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	ExtractedView& outView);
