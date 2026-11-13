#pragma once

#include "Runtime/Engine/World/Component/CameraComponent.h"
#include "Runtime/Engine/Camera/CameraState.h"

#include <DirectXMath.h>

#include <cstdint>

struct ExtractedView;

void BuildExtractedView(
	const CameraState& camera,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	ExtractedView& outView);

// Position and orientation come from the camera world matrix. Lens comes from the component.
void BuildExtractedView(
	DirectX::FXMMATRIX worldMatrix,
	const CameraComponent& camera,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	ExtractedView& outView);
