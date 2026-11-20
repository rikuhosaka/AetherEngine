#include "Runtime/Renderer/Scene/FrustumCull.h"

#include "Runtime/Core/Math/Matrix.h"

#include <DirectXMath.h>

#include <cmath>

namespace
{
struct Plane
{
	float normal[3]{};
	float distance = 0.0f;
};

[[nodiscard]] Plane LoadPlane(const float stored[4])
{
	Plane plane{};
	plane.normal[0] = stored[0];
	plane.normal[1] = stored[1];
	plane.normal[2] = stored[2];
	plane.distance = stored[3];
	return plane;
}

void StorePlane(float destination[4], DirectX::FXMVECTOR row)
{
	DirectX::XMFLOAT4 stored{};
	DirectX::XMStoreFloat4(&stored, row);
	const float length = std::sqrt(
		(stored.x * stored.x) + (stored.y * stored.y) + (stored.z * stored.z));
	if (length < 1.0e-8f)
	{
		destination[0] = 0.0f;
		destination[1] = 0.0f;
		destination[2] = 0.0f;
		destination[3] = 0.0f;
		return;
	}

	const float inverse = 1.0f / length;
	destination[0] = stored.x * inverse;
	destination[1] = stored.y * inverse;
	destination[2] = stored.z * inverse;
	destination[3] = stored.w * inverse;
}

[[nodiscard]] bool IsOutside(const Plane& plane, const MeshBounds& bounds)
{
	const float positiveX = plane.normal[0] >= 0.0f ? bounds.maxX : bounds.minX;
	const float positiveY = plane.normal[1] >= 0.0f ? bounds.maxY : bounds.minY;
	const float positiveZ = plane.normal[2] >= 0.0f ? bounds.maxZ : bounds.minZ;
	const float distance =
		(plane.normal[0] * positiveX) +
		(plane.normal[1] * positiveY) +
		(plane.normal[2] * positiveZ) +
		plane.distance;
	return distance < 0.0f;
}
} // namespace

bool AreBoundsCullable(const MeshBounds& bounds)
{
	return BoundsAreValid(bounds);
}

ViewFrustum MakeViewFrustum(const float viewProjectionMatrix[16])
{
	using namespace DirectX;

	// Clip = rowVector * viewProjection, so the clip planes come from the columns.
	const XMMATRIX clipColumns = XMMatrixTranspose(Aether::Math::LoadMatrixFromHlsl(viewProjectionMatrix));
	ViewFrustum frustum{};
	StorePlane(frustum.planes[0], XMVectorAdd(clipColumns.r[3], clipColumns.r[0]));
	StorePlane(frustum.planes[1], XMVectorSubtract(clipColumns.r[3], clipColumns.r[0]));
	StorePlane(frustum.planes[2], XMVectorAdd(clipColumns.r[3], clipColumns.r[1]));
	StorePlane(frustum.planes[3], XMVectorSubtract(clipColumns.r[3], clipColumns.r[1]));
	StorePlane(frustum.planes[4], clipColumns.r[2]);
	StorePlane(frustum.planes[5], XMVectorSubtract(clipColumns.r[3], clipColumns.r[2]));
	return frustum;
}

bool IntersectsViewFrustum(
	const ViewFrustum& frustum,
	const float worldMatrix[16],
	const MeshBounds& localBounds)
{
	using namespace DirectX;

	const XMMATRIX world = Aether::Math::LoadMatrixFromHlsl(worldMatrix);
	const XMVECTOR corners[8] = {
		XMVectorSet(localBounds.minX, localBounds.minY, localBounds.minZ, 1.0f),
		XMVectorSet(localBounds.maxX, localBounds.minY, localBounds.minZ, 1.0f),
		XMVectorSet(localBounds.minX, localBounds.maxY, localBounds.minZ, 1.0f),
		XMVectorSet(localBounds.maxX, localBounds.maxY, localBounds.minZ, 1.0f),
		XMVectorSet(localBounds.minX, localBounds.minY, localBounds.maxZ, 1.0f),
		XMVectorSet(localBounds.maxX, localBounds.minY, localBounds.maxZ, 1.0f),
		XMVectorSet(localBounds.minX, localBounds.maxY, localBounds.maxZ, 1.0f),
		XMVectorSet(localBounds.maxX, localBounds.maxY, localBounds.maxZ, 1.0f),
	};

	MeshBounds worldBounds{};
	ResetBounds(worldBounds);
	for (const XMVECTOR corner : corners)
	{
		XMFLOAT3 transformed{};
		XMStoreFloat3(&transformed, XMVector3TransformCoord(corner, world));
		const float position[3] = { transformed.x, transformed.y, transformed.z };
		ExpandBounds(worldBounds, position);
	}

	for (const float (&storedPlane)[4] : frustum.planes)
	{
		if (IsOutside(LoadPlane(storedPlane), worldBounds))
		{
			return false;
		}
	}

	return true;
}
