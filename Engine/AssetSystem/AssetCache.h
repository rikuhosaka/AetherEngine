#pragma once

#include "Engine/AssetSystem/MeshAsset.h"
#include "Engine/Core/Log/Result.h"

#include <filesystem>
#include <string_view>
#include <vector>

// Reads one mesh JSON. The path must be relative to assetsRoot and stay under Meshes/.
[[nodiscard]] Result<MeshAsset> LoadMeshAssetFile(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath);

// Keeps mesh JSON in memory. LoadDirectory reads each id once.
class AssetCache
{
public:
	[[nodiscard]] Result<void> LoadDirectory(const std::filesystem::path& assetsRoot);

	[[nodiscard]] const MeshAsset* Find(std::string_view id) const;

private:
	std::vector<MeshAsset> m_assets{};
	bool m_loaded = false;
};
