#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/World/SceneDocument.h"

#include <filesystem>
#include <vector>

class FrameContext;
class RenderResourceServices;
class RHICommandList;

struct SceneAssets
{
	std::vector<SceneMeshRef> meshes{};
	SceneRenderResources render{};
};

// Uploads one mesh per document mesh key from that key's rigid body shape.
class SceneAssetLoader
{
public:
	[[nodiscard]] Result<SceneAssets> Load(
		const SceneDocument& document,
		RenderResourceServices& resources,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& shaderRoot);
};
