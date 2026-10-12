#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Mesh/MeshUpload.h"
#include "Engine/Renderer/Model/Loader/ModelLoadTypes.h"

#include <DirectXMath.h>

#include <span>
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

// halfHeight is the cylindrical section's half height. Total half extent is halfHeight + radius.
struct CapsulePrimitiveDesc
{
	float radius = 0.25f;
	float halfHeight = 0.5f;
	uint32_t slices = 16;
	uint32_t hemisphereStacks = 4;
};

struct ConvexHullPrimitiveDesc
{
	std::span<const DirectX::XMFLOAT3> points{};
};

// indices are outward triangles in engine space. Degenerate triangles are dropped.
struct TriangleMeshPrimitiveDesc
{
	std::span<const DirectX::XMFLOAT3> points{};
	std::span<const uint32_t> indices{};
};

// XZ plane centered at the origin, facing +Y.
[[nodiscard]] Result<PrimitiveMeshData> GeneratePlaneMesh(const PlanePrimitiveDesc& desc = {});

// Axis-aligned box centered at the origin. Faces point outward unless inwardFacing is set.
[[nodiscard]] Result<PrimitiveMeshData> GenerateCubeMesh(const CubePrimitiveDesc& desc = {});

// UV sphere centered at the origin. Normals point outward.
[[nodiscard]] Result<PrimitiveMeshData> GenerateSphereMesh(const SpherePrimitiveDesc& desc = {});

// Y-axis capsule centered at the origin. Normals point outward.
[[nodiscard]] Result<PrimitiveMeshData> GenerateCapsuleMesh(const CapsulePrimitiveDesc& desc = {});

// Closed convex hull of the supplied points, with one flat normal per face.
[[nodiscard]] Result<PrimitiveMeshData> GenerateConvexHullMesh(const ConvexHullPrimitiveDesc& desc);

// Triangle list with one flat normal per face. Winding is kept as given.
[[nodiscard]] Result<PrimitiveMeshData> GenerateTriangleMesh(const TriangleMeshPrimitiveDesc& desc);

[[nodiscard]] MeshUploadDesc MakePrimitiveMeshUploadDesc(
	const PrimitiveMeshData& mesh,
	const char* debugName);
