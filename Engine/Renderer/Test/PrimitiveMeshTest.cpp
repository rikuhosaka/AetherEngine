#include "Engine/Renderer/Test/PrimitiveMeshTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Renderer/Mesh/PrimitiveMesh.h"

#include <cmath>

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

	LOG_INFO(LogCategory::Renderer, "PrimitiveMeshTests passed");
	return MakeOk();
}
