#include "Engine/Renderer/Test/PrimitiveMeshTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Renderer/Mesh/PrimitiveMesh.h"

#include <DirectXMath.h>

#include <array>
#include <cmath>
#include <span>

namespace
{
[[nodiscard]] bool IsNearlyEqual(float left, float right, float epsilon = 0.0001f)
{
	return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] Result<void> ExpectTriangleWindingMatchesNormal(const PrimitiveMeshData& mesh)
{
	if (mesh.indices.size() < 3)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Primitive mesh has no triangles");
	}

	const BasicVertex& v0 = mesh.vertices[mesh.indices[0]];
	const BasicVertex& v1 = mesh.vertices[mesh.indices[1]];
	const BasicVertex& v2 = mesh.vertices[mesh.indices[2]];

	const float e1x = v1.position[0] - v0.position[0];
	const float e1y = v1.position[1] - v0.position[1];
	const float e1z = v1.position[2] - v0.position[2];
	const float e2x = v2.position[0] - v0.position[0];
	const float e2y = v2.position[1] - v0.position[1];
	const float e2z = v2.position[2] - v0.position[2];
	const float crossX = (e1y * e2z) - (e1z * e2y);
	const float crossY = (e1z * e2x) - (e1x * e2z);
	const float crossZ = (e1x * e2y) - (e1y * e2x);
	const float alignment =
		(crossX * v0.normal[0]) + (crossY * v0.normal[1]) + (crossZ * v0.normal[2]);
	if (alignment <= 0.0f)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Primitive triangle winding does not match vertex normal");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> ValidatePlane()
{
	const Result<PrimitiveMeshData> planeResult = GeneratePlaneMesh({ 2.0f, 4.0f });
	if (!planeResult)
	{
		return MakeFail(planeResult.error.code, planeResult.error.message);
	}

	const PrimitiveMeshData& plane = planeResult.value;
	if (plane.vertices.size() != 4 || plane.indices.size() != 6)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Plane primitive vertex or index count is incorrect");
	}

	for (const BasicVertex& vertex : plane.vertices)
	{
		if (!IsNearlyEqual(vertex.normal[0], 0.0f) ||
			!IsNearlyEqual(vertex.normal[1], 1.0f) ||
			!IsNearlyEqual(vertex.normal[2], 0.0f))
		{
			return FailRuntime(
				LogCategory::Renderer,
				ErrorCode::InvalidArgument,
				"Plane primitive normal is not +Y");
		}

		if (vertex.tangent[0] <= 0.5f || !IsNearlyEqual(vertex.tangent[3], 1.0f))
		{
			return FailRuntime(
				LogCategory::Renderer,
				ErrorCode::InvalidArgument,
				"Plane primitive tangent should follow +X with positive handedness");
		}
	}

	if (!IsNearlyEqual(plane.bounds.minX, -1.0f) ||
		!IsNearlyEqual(plane.bounds.maxX, 1.0f) ||
		!IsNearlyEqual(plane.bounds.minZ, -2.0f) ||
		!IsNearlyEqual(plane.bounds.maxZ, 2.0f) ||
		!IsNearlyEqual(plane.bounds.minY, 0.0f) ||
		!IsNearlyEqual(plane.bounds.maxY, 0.0f))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Plane primitive bounds are incorrect");
	}

	return ExpectTriangleWindingMatchesNormal(plane);
}

[[nodiscard]] Result<void> ValidateCube(bool inwardFacing)
{
	const Result<PrimitiveMeshData> cubeResult = GenerateCubeMesh({ 1.0f, 1.0f, 1.0f, inwardFacing });
	if (!cubeResult)
	{
		return MakeFail(cubeResult.error.code, cubeResult.error.message);
	}

	const PrimitiveMeshData& cube = cubeResult.value;
	if (cube.vertices.size() != 24 || cube.indices.size() != 36)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Cube primitive vertex or index count is incorrect");
	}

	if (!IsNearlyEqual(cube.bounds.minX, -0.5f) ||
		!IsNearlyEqual(cube.bounds.maxX, 0.5f) ||
		!IsNearlyEqual(cube.bounds.minY, -0.5f) ||
		!IsNearlyEqual(cube.bounds.maxY, 0.5f) ||
		!IsNearlyEqual(cube.bounds.minZ, -0.5f) ||
		!IsNearlyEqual(cube.bounds.maxZ, 0.5f))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Cube primitive bounds are incorrect");
	}

	const float expectedYNormal = inwardFacing ? -1.0f : 1.0f;
	uint32_t topFaceVertexCount = 0;
	for (const BasicVertex& vertex : cube.vertices)
	{
		if (!IsNearlyEqual(vertex.normal[0], 0.0f) ||
			!IsNearlyEqual(vertex.normal[1], expectedYNormal) ||
			!IsNearlyEqual(vertex.normal[2], 0.0f))
		{
			continue;
		}

		++topFaceVertexCount;
		if (!IsNearlyEqual(vertex.position[1], 0.5f))
		{
			return FailRuntime(
				LogCategory::Renderer,
				ErrorCode::InvalidArgument,
				inwardFacing
					? "Inward cube -Y face is not on the top"
					: "Cube +Y face is not on the top");
		}
	}

	if (topFaceVertexCount != 4)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Cube primitive is missing a top face");
	}

	return ExpectTriangleWindingMatchesNormal(cube);
}

[[nodiscard]] Result<void> ValidateSphere()
{
	const Result<PrimitiveMeshData> sphereResult = GenerateSphereMesh({ 0.5f, 12, 16 });
	if (!sphereResult)
	{
		return MakeFail(sphereResult.error.code, sphereResult.error.message);
	}

	const PrimitiveMeshData& sphere = sphereResult.value;
	if (sphere.vertices.size() != 178 || sphere.indices.size() != 1056)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Sphere primitive vertex or index count is incorrect");
	}

	if (!IsNearlyEqual(sphere.bounds.minX, -0.5f) ||
		!IsNearlyEqual(sphere.bounds.maxX, 0.5f) ||
		!IsNearlyEqual(sphere.bounds.minY, -0.5f) ||
		!IsNearlyEqual(sphere.bounds.maxY, 0.5f) ||
		!IsNearlyEqual(sphere.bounds.minZ, -0.5f) ||
		!IsNearlyEqual(sphere.bounds.maxZ, 0.5f))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Sphere primitive bounds are incorrect");
	}

	for (const BasicVertex& vertex : sphere.vertices)
	{
		const float length = std::sqrt(
			(vertex.position[0] * vertex.position[0])
			+ (vertex.position[1] * vertex.position[1])
			+ (vertex.position[2] * vertex.position[2]));
		if (!IsNearlyEqual(length, 0.5f))
		{
			return FailRuntime(
				LogCategory::Renderer,
				ErrorCode::InvalidArgument,
				"Sphere primitive vertex is not on the radius");
		}

		const float normalAlignment =
			(vertex.normal[0] * vertex.position[0])
			+ (vertex.normal[1] * vertex.position[1])
			+ (vertex.normal[2] * vertex.position[2]);
		if (normalAlignment <= 0.0f)
		{
			return FailRuntime(
				LogCategory::Renderer,
				ErrorCode::InvalidArgument,
				"Sphere primitive normal does not point outward");
		}
	}

	for (size_t index = 0; index + 2 < sphere.indices.size(); index += 3)
	{
		const BasicVertex& v0 = sphere.vertices[sphere.indices[index]];
		const BasicVertex& v1 = sphere.vertices[sphere.indices[index + 1]];
		const BasicVertex& v2 = sphere.vertices[sphere.indices[index + 2]];
		const float edge1X = v1.position[0] - v0.position[0];
		const float edge1Y = v1.position[1] - v0.position[1];
		const float edge1Z = v1.position[2] - v0.position[2];
		const float edge2X = v2.position[0] - v0.position[0];
		const float edge2Y = v2.position[1] - v0.position[1];
		const float edge2Z = v2.position[2] - v0.position[2];
		const float crossX = (edge1Y * edge2Z) - (edge1Z * edge2Y);
		const float crossY = (edge1Z * edge2X) - (edge1X * edge2Z);
		const float crossZ = (edge1X * edge2Y) - (edge1Y * edge2X);
		const float alignment =
			(crossX * v0.normal[0]) + (crossY * v0.normal[1]) + (crossZ * v0.normal[2]);
		if (alignment <= 0.0f)
		{
			return FailRuntime(
				LogCategory::Renderer,
				ErrorCode::InvalidArgument,
				"Sphere primitive triangle winding does not match vertex normal");
		}
	}

	return MakeOk();
}

[[nodiscard]] Result<void> ExpectEveryTriangleFacesOutward(const PrimitiveMeshData& mesh)
{
	if (mesh.indices.size() < 3 || (mesh.indices.size() % 3) != 0)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Primitive mesh triangle list is incomplete");
	}

	for (size_t index = 0; index + 2 < mesh.indices.size(); index += 3)
	{
		const BasicVertex& v0 = mesh.vertices[mesh.indices[index]];
		const BasicVertex& v1 = mesh.vertices[mesh.indices[index + 1]];
		const BasicVertex& v2 = mesh.vertices[mesh.indices[index + 2]];
		const float e1x = v1.position[0] - v0.position[0];
		const float e1y = v1.position[1] - v0.position[1];
		const float e1z = v1.position[2] - v0.position[2];
		const float e2x = v2.position[0] - v0.position[0];
		const float e2y = v2.position[1] - v0.position[1];
		const float e2z = v2.position[2] - v0.position[2];
		const float crossX = (e1y * e2z) - (e1z * e2y);
		const float crossY = (e1z * e2x) - (e1x * e2z);
		const float crossZ = (e1x * e2y) - (e1y * e2x);
		const float alignment =
			(crossX * v0.normal[0]) + (crossY * v0.normal[1]) + (crossZ * v0.normal[2]);
		const float centerX = (v0.position[0] + v1.position[0] + v2.position[0]) / 3.0f;
		const float centerY = (v0.position[1] + v1.position[1] + v2.position[1]) / 3.0f;
		const float centerZ = (v0.position[2] + v1.position[2] + v2.position[2]) / 3.0f;
		const float outward =
			(centerX * v0.normal[0]) + (centerY * v0.normal[1]) + (centerZ * v0.normal[2]);
		if (alignment <= 0.0f || outward <= 0.0f)
		{
			return FailRuntime(
				LogCategory::Renderer,
				ErrorCode::InvalidArgument,
				"Primitive triangle does not face outward");
		}
	}

	return MakeOk();
}

[[nodiscard]] Result<void> ValidateCapsule()
{
	constexpr float kRadius = 0.25f;
	constexpr float kHalfHeight = 0.5f;
	const Result<PrimitiveMeshData> capsuleResult = GenerateCapsuleMesh({ kRadius, kHalfHeight, 8, 3 });
	if (!capsuleResult)
	{
		return MakeFail(capsuleResult.error.code, capsuleResult.error.message);
	}

	const PrimitiveMeshData& capsule = capsuleResult.value;
	const float extent = kRadius + kHalfHeight;
	if (!IsNearlyEqual(capsule.bounds.minY, -extent) ||
		!IsNearlyEqual(capsule.bounds.maxY, extent) ||
		!IsNearlyEqual(capsule.bounds.minX, -kRadius) ||
		!IsNearlyEqual(capsule.bounds.maxX, kRadius) ||
		!IsNearlyEqual(capsule.bounds.minZ, -kRadius) ||
		!IsNearlyEqual(capsule.bounds.maxZ, kRadius))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Capsule primitive bounds do not match radius and half height");
	}

	return ExpectEveryTriangleFacesOutward(capsule);
}

[[nodiscard]] Result<void> ValidateConvexHull()
{
	constexpr float kHalf = 0.25f;
	const std::array<DirectX::XMFLOAT3, 8> corners = {{
		{ -kHalf, -kHalf, -kHalf },
		{ kHalf, -kHalf, -kHalf },
		{ kHalf, kHalf, -kHalf },
		{ -kHalf, kHalf, -kHalf },
		{ -kHalf, -kHalf, kHalf },
		{ kHalf, -kHalf, kHalf },
		{ kHalf, kHalf, kHalf },
		{ -kHalf, kHalf, kHalf },
	}};
	const Result<PrimitiveMeshData> hullResult = GenerateConvexHullMesh({ corners });
	if (!hullResult)
	{
		return MakeFail(hullResult.error.code, hullResult.error.message);
	}

	const PrimitiveMeshData& hull = hullResult.value;
	if (hull.indices.size() != 36)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Cube convex hull should be twelve triangles");
	}

	if (!IsNearlyEqual(hull.bounds.minX, -kHalf) ||
		!IsNearlyEqual(hull.bounds.maxX, kHalf) ||
		!IsNearlyEqual(hull.bounds.minY, -kHalf) ||
		!IsNearlyEqual(hull.bounds.maxY, kHalf) ||
		!IsNearlyEqual(hull.bounds.minZ, -kHalf) ||
		!IsNearlyEqual(hull.bounds.maxZ, kHalf))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Convex hull bounds do not cover the source points");
	}

	return ExpectEveryTriangleFacesOutward(hull);
}

[[nodiscard]] Result<void> ValidateTriangleMesh()
{
	const std::array<DirectX::XMFLOAT3, 4> corners = {{
		{ -2.0f, 0.0f, -2.0f },
		{ -2.0f, 0.0f, 2.0f },
		{ 2.0f, 0.0f, -2.0f },
		{ 2.0f, 0.0f, 2.0f },
	}};
	const std::array<uint32_t, 6> indices = { 0, 1, 2, 3, 2, 1 };
	const Result<PrimitiveMeshData> meshResult = GenerateTriangleMesh({ corners, indices });
	if (!meshResult)
	{
		return MakeFail(meshResult.error.code, meshResult.error.message);
	}

	const PrimitiveMeshData& mesh = meshResult.value;
	if (mesh.vertices.size() != 6 || mesh.indices.size() != 6)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Triangle mesh should keep both source triangles");
	}

	for (const BasicVertex& vertex : mesh.vertices)
	{
		if (!IsNearlyEqual(vertex.normal[0], 0.0f) ||
			!IsNearlyEqual(vertex.normal[1], 1.0f) ||
			!IsNearlyEqual(vertex.normal[2], 0.0f))
		{
			return FailRuntime(
				LogCategory::Renderer,
				ErrorCode::InvalidArgument,
				"Triangle mesh floor normal is not +Y");
		}
	}

	if (!IsNearlyEqual(mesh.bounds.minX, -2.0f) ||
		!IsNearlyEqual(mesh.bounds.maxX, 2.0f) ||
		!IsNearlyEqual(mesh.bounds.minZ, -2.0f) ||
		!IsNearlyEqual(mesh.bounds.maxZ, 2.0f))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Triangle mesh bounds do not cover the source points");
	}

	return ExpectTriangleWindingMatchesNormal(mesh);
}
} // namespace

Result<void> RunPrimitiveMeshTests()
{
	if (auto planeResult = ValidatePlane(); !planeResult)
	{
		return planeResult;
	}

	if (auto cubeResult = ValidateCube(false); !cubeResult)
	{
		return cubeResult;
	}

	if (auto inwardCubeResult = ValidateCube(true); !inwardCubeResult)
	{
		return inwardCubeResult;
	}

	if (auto sphereResult = ValidateSphere(); !sphereResult)
	{
		return sphereResult;
	}

	if (auto capsuleResult = ValidateCapsule(); !capsuleResult)
	{
		return capsuleResult;
	}

	if (auto hullResult = ValidateConvexHull(); !hullResult)
	{
		return hullResult;
	}

	if (auto triangleResult = ValidateTriangleMesh(); !triangleResult)
	{
		return triangleResult;
	}

	if (GeneratePlaneMesh({ 0.0f, 1.0f }))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"GeneratePlaneMesh should reject non-positive size");
	}

	if (GenerateCubeMesh({ 1.0f, -1.0f, 1.0f }))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"GenerateCubeMesh should reject non-positive size");
	}

	if (GenerateSphereMesh({ 0.0f, 12, 16 }) || GenerateSphereMesh({ 0.5f, 1, 16 }) || GenerateSphereMesh({ 0.5f, 12, 2 }))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"GenerateSphereMesh should reject a non-positive radius or too few segments");
	}

	if (GenerateCapsuleMesh({ 0.0f, 0.5f, 8, 3 }) || GenerateCapsuleMesh({ 0.25f, 0.0f, 8, 3 }) || GenerateCapsuleMesh({ 0.25f, 0.5f, 2, 3 }))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"GenerateCapsuleMesh should reject a non-positive size or too few slices");
	}

	const std::array<DirectX::XMFLOAT3, 4> flat = {{
		{ 0.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 1.0f, 0.0f, 1.0f },
	}};
	if (GenerateConvexHullMesh({ std::span<const DirectX::XMFLOAT3>(flat.data(), 3) }) || GenerateConvexHullMesh({ flat }))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"GenerateConvexHullMesh should reject fewer than four points and a flat point set");
	}

	const std::array<uint32_t, 3> missing = { 0, 1, 4 };
	if (GenerateTriangleMesh({ flat, std::span<const uint32_t>(missing.data(), 2) }) || GenerateTriangleMesh({ flat, missing }))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"GenerateTriangleMesh should reject a short index list and an out-of-range index");
	}

	LOG_INFO(LogCategory::Renderer, "PrimitiveMeshTests passed");
	return MakeOk();
}
