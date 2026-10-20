#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Scene/SceneDocument.h"

#include <filesystem>
#include <vector>

class AssetCache;
class FrameContext;
class RenderResourceServices;
class RHICommandList;

struct SceneAssets
{
	std::vector<SceneMeshRef> meshes{};
	SceneRenderResources render{};
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
		const std::filesystem::path& shaderRoot);
};
