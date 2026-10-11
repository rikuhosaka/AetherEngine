#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Mesh/MeshUpload.h"
#include "Engine/Renderer/Model/Loader/ModelLoadTypes.h"

#include <vector>

struct PrimitiveMeshData
{
	std::vector<BasicVertex> vertices{};
	std::vector<uint32_t> indices{};
	MeshBounds bounds{};
};

struct PlanePrimitiveDesc
{
	float width = 1.0f;
	float depth = 1.0f;
};

struct CubePrimitiveDesc
{
	float width = 1.0f;
	float height = 1.0f;
	float depth = 1.0f;
	bool inwardFacing = false;
};

struct SpherePrimitiveDesc
{
	float radius = 0.5f;
	uint32_t stacks = 12;
	uint32_t slices = 16;
};

// XZ plane centered at the origin, facing +Y.
[[nodiscard]] Result<PrimitiveMeshData> GeneratePlaneMesh(const PlanePrimitiveDesc& desc = {});

// Axis-aligned box centered at the origin. Faces point outward unless inwardFacing is set.
[[nodiscard]] Result<PrimitiveMeshData> GenerateCubeMesh(const CubePrimitiveDesc& desc = {});

// UV sphere centered at the origin. Normals point outward.
[[nodiscard]] Result<PrimitiveMeshData> GenerateSphereMesh(const SpherePrimitiveDesc& desc = {});

[[nodiscard]] MeshUploadDesc MakePrimitiveMeshUploadDesc(
	const PrimitiveMeshData& mesh,
	const char* debugName);
