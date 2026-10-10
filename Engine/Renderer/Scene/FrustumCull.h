#pragma once

#include "Engine/Renderer/Mesh/MeshTypes.h"

struct ViewFrustum
{
	float planes[6][4]{};
};

[[nodiscard]] bool AreBoundsCullable(const MeshBounds& bounds);

// viewProjectionMatrix and worldMatrix use the HLSL layout produced by StoreMatrixForHlsl.
[[nodiscard]] ViewFrustum MakeViewFrustum(const float viewProjectionMatrix[16]);

[[nodiscard]] bool IntersectsViewFrustum(
	const ViewFrustum& frustum,
	const float worldMatrix[16],
	const MeshBounds& localBounds);
