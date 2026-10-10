#pragma once

#include "Engine/Renderer/Mesh/MeshTypes.h"

[[nodiscard]] bool AreBoundsCullable(const MeshBounds& bounds);

// viewProjectionMatrix and worldMatrix use the HLSL layout produced by StoreMatrixForHlsl.
[[nodiscard]] bool IntersectsViewFrustum(
	const float viewProjectionMatrix[16],
	const float worldMatrix[16],
	const MeshBounds& localBounds);
