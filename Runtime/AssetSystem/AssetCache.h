#pragma once

#include "Runtime/AssetSystem/Mesh/CookedMesh.h"
#include "Runtime/Core/Log/Result.h"

#include <filesystem>
#include <string_view>
#include <vector>

// Reads one mesh JSON. The path must be relative to assetsRoot and stay under Meshes/.
[[nodiscard]] Result<MeshAsset> LoadMeshAssetFile(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath);

// Loads each mesh JSON once. Reuses a cooked render mesh when the source bytes still match.
class AssetCache
{
public:
	[[nodiscard]] Result<void> LoadDirectory(const std::filesystem::path& assetsRoot);

	[[nodiscard]] const MeshAsset* Find(std::string_view id) const;
	[[nodiscard]] const PrimitiveMeshData* FindRender(std::string_view id) const;

private:
	std::vector<CookedMesh> m_meshes{};
	bool m_loaded = false;
};
