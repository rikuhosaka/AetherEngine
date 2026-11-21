#pragma once

#include "Runtime/AssetData/ModelData.h"
#include "Runtime/AssetData/PrimitiveMeshData.h"
#include "Runtime/AssetData/TextureData.h"
#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/SceneDocument/SceneDocument.h"
#include "Runtime/Renderer/Scene/SceneGpuUpload.h"

#include <filesystem>
#include <string>
#include <vector>

class AssetCache;

// Owns the CPU bytes that SceneGpuUpload descs point at. Keep it alive through UploadScene.
// Do not move it after BuildSceneGpuUpload; the upload spans point into these vectors.
struct SceneGpuUploadSources
{
	std::vector<PrimitiveMeshData> primitives{};
	std::vector<ModelAssetData> models{};
	std::vector<TextureImageData> textures{};
	std::vector<std::string> textureCacheKeys{};
	std::vector<std::string> textureDebugNames{};
	SceneGpuUpload upload{};
};

[[nodiscard]] Result<void> BuildSceneGpuUpload(
	const SceneDocument& document,
	const AssetCache& cache,
	const std::filesystem::path& assetsRoot,
	SceneGpuUploadSources& out);
