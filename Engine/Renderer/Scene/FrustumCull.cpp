#include "Engine/Renderer/Scene/FrustumCull.h"

#include "Engine/Math/Matrix.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace
{
struct Plane
{
	float normal[3]{};
	float distance = 0.0f;
};

[[nodiscard]] Plane MakePlane(DirectX::FXMVECTOR row)
{
	DirectX::XMFLOAT4 stored{};
	DirectX::XMStoreFloat4(&stored, row);
	const float length = std::sqrt(
		(stored.x * stored.x) + (stored.y * stored.y) + (stored.z * stored.z));
	Plane plane{};
	if (length < 1.0e-8f)
	{
		return plane;
	}

	const float inverse = 1.0f / length;
	plane.normal[0] = stored.x * inverse;
	plane.normal[1] = stored.y * inverse;
	plane.normal[2] = stored.z * inverse;
	plane.distance = stored.w * inverse;
	return plane;
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
	return bounds.minX <= bounds.maxX && bounds.minY <= bounds.maxY && bounds.minZ <= bounds.maxZ;
}

bool IntersectsViewFrustum(
	const float viewProjectionMatrix[16],
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
	worldBounds.minX = worldBounds.minY = worldBounds.minZ = FLT_MAX;
	worldBounds.maxX = worldBounds.maxY = worldBounds.maxZ = -FLT_MAX;
	for (const XMVECTOR corner : corners)
	{
		XMFLOAT3 transformed{};
		XMStoreFloat3(&transformed, XMVector3TransformCoord(corner, world));
		worldBounds.minX = (std::min)(worldBounds.minX, transformed.x);
		worldBounds.minY = (std::min)(worldBounds.minY, transformed.y);
		worldBounds.minZ = (std::min)(worldBounds.minZ, transformed.z);
		worldBounds.maxX = (std::max)(worldBounds.maxX, transformed.x);
		worldBounds.maxY = (std::max)(worldBounds.maxY, transformed.y);
		worldBounds.maxZ = (std::max)(worldBounds.maxZ, transformed.z);
	}

	// Clip = rowVector * viewProjection, so the clip planes come from the columns.
	const XMMATRIX clipColumns = XMMatrixTranspose(Aether::Math::LoadMatrixFromHlsl(viewProjectionMatrix));
	const Plane planes[6] = {
		MakePlane(XMVectorAdd(clipColumns.r[3], clipColumns.r[0])),
		MakePlane(XMVectorSubtract(clipColumns.r[3], clipColumns.r[0])),
		MakePlane(XMVectorAdd(clipColumns.r[3], clipColumns.r[1])),
		MakePlane(XMVectorSubtract(clipColumns.r[3], clipColumns.r[1])),
		MakePlane(clipColumns.r[2]),
		MakePlane(XMVectorSubtract(clipColumns.r[3], clipColumns.r[2])),
	};

	for (const Plane& plane : planes)
	{
		if (IsOutside(plane, worldBounds))
		{
			return false;
		}
	}

	return true;
}
