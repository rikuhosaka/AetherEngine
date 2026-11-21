#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/Material/MaterialTypes.h"
#include "Runtime/Renderer/Mesh/MeshTypes.h"
#include "Runtime/Renderer/Scene/FbxSceneAssets.h"
#include "Runtime/Renderer/Scene/SceneGpuUpload.h"
#include "Runtime/Renderer/Texture/TextureTypes.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class FrameContext;
class RenderResourceServices;
class RHICommandList;

struct SceneModelAsset
{
	uint32_t documentId = 0;
	std::unique_ptr<FbxSceneAssets> assets{};
};

struct UploadedMeshRef
{
	std::string key{};
	MeshHandle mesh{};
};

struct UploadedRenderResources
{
	MaterialHandle material{};
	TextureHandle baseColor{};
	std::optional<uint32_t> materialConstantsSlot{};
};

struct UploadedSceneAssets
{
	std::vector<UploadedMeshRef> meshes{};
	UploadedRenderResources render{};
	std::vector<SceneModelAsset> models{};
};

// Uploads meshes, textures, and the shared lit material from CPU upload data.
[[nodiscard]] Result<UploadedSceneAssets> UploadSceneAssets(
	const SceneGpuUpload& upload,
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot);
