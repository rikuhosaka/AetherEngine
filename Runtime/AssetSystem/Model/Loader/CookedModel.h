#pragma once

#include "Runtime/AssetSystem/Model/Loader/ModelLoadTypes.h"
#include "Runtime/Core/Log/Result.h"

#include <cstdint>
#include <filesystem>

// Bump when ModelAssetData, BasicVertex, or the import options change shape.
constexpr uint32_t kCookedModelVersion = 1;

// Cooked file for one model. relativePath is relative to assetsRoot, including its extension.
[[nodiscard]] std::filesystem::path CookedModelPath(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath);

// False when the file is missing, stale, or incomplete. Texture paths are restored under assetsRoot.
[[nodiscard]] bool TryReadCookedModel(
	const std::filesystem::path& path,
	uint64_t sourceHash,
	const FbxModelLoadOptions& options,
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	ModelAssetData& outModel);

[[nodiscard]] Result<void> WriteCookedModel(
	const std::filesystem::path& path,
	uint64_t sourceHash,
	const FbxModelLoadOptions& options,
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	const ModelAssetData& model);

// Reads a cooked model when the source bytes and import options still match. Otherwise imports and writes the cook.
[[nodiscard]] Result<ModelAssetData> LoadModelAsset(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	const FbxModelLoadOptions& options = {});
