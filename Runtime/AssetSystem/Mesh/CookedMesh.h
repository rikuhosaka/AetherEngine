#pragma once

#include "Runtime/AssetSystem/Mesh/PrimitiveMesh.h"
#include "Runtime/AssetSystem/MeshAsset.h"
#include "Runtime/Core/Log/Result.h"

#include <cstdint>
#include <filesystem>

// Bump when BasicVertex, MeshAsset fields, or primitive generation change.
constexpr uint32_t kCookedMeshVersion = 1;

// Source description plus the generated render mesh.
struct CookedMesh
{
	MeshAsset source{};
	PrimitiveMeshData render{};
};

// Cooked file for a JSON mesh. jsonFileName is the file name under Assets/Meshes/.
[[nodiscard]] std::filesystem::path CookedMeshPath(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& jsonFileName);

// False when the file is missing, stale, or incomplete. The caller then cooks again.
[[nodiscard]] bool TryReadCookedMesh(
	const std::filesystem::path& path,
	uint64_t sourceHash,
	CookedMesh& outMesh);

[[nodiscard]] Result<void> WriteCookedMesh(
	const std::filesystem::path& path,
	uint64_t sourceHash,
	const CookedMesh& mesh);
