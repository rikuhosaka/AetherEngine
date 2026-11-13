#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/Scene/FbxSceneAssets.h"
#include "Runtime/Engine/SceneDocument/SceneDocument.h"

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

struct UploadedSceneAssets
{
	std::vector<SceneMeshRef> meshes{};
	SceneRenderResources render{};
	std::vector<SceneModelAsset> models{};
};

// Uploads one mesh per document mesh key, model textures, and the shared lit material.
[[nodiscard]] Result<UploadedSceneAssets> UploadSceneAssets(
	const SceneDocument& document,
	const AssetCache& cache,
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& shaderRoot);
