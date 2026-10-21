#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Scene/FbxSceneAssets.h"
#include "Engine/Scene/SceneDocument.h"

#include <filesystem>
#include <memory>
#include <vector>

class AssetCache;
class FrameContext;
class RenderResourceServices;
class RHICommandList;

struct SceneModelAsset
{
	uint32_t documentId = 0;
	std::unique_ptr<FbxSceneAssets> assets{};
};

struct SceneAssets
{
	std::vector<SceneMeshRef> meshes{};
	SceneRenderResources render{};
	std::vector<SceneModelAsset> models{};
};

// Copies mesh and convex-hull collision points from the asset cache onto authored bodies.
[[nodiscard]] Result<void> ApplyMeshCollision(SceneDocument& document, const AssetCache& cache);

// Uploads one mesh per document mesh key from the asset cache.
class SceneAssetLoader
{
public:
	[[nodiscard]] Result<SceneAssets> Load(
		const SceneDocument& document,
		const AssetCache& cache,
		RenderResourceServices& resources,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& assetsRoot,
		const std::filesystem::path& shaderRoot);
};
