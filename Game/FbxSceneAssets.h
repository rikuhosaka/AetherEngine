#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/Renderer/Model/Loader/FbxSdkContext.h"
#include "Engine/Renderer/Scene/RenderObjectId.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/Renderer/Texture/TextureTypes.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

class FrameContext;
class Renderer;
class RHICommandList;

struct LitMaterialConstants
{
	float tint[4] { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct FbxSceneInstance
{
	RenderObjectId objectId{};
	MeshHandle mesh{};
	TextureHandle baseColor{};
	uint32_t submeshIndex = 0;
	float worldMatrix[16]{};
};

class FbxSceneAssets
{
public:
	[[nodiscard]] bool IsReady() const noexcept { return m_ready; }

	[[nodiscard]] Result<void> EnsureInitialized(
		Renderer& renderer,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& shaderRoot,
		const std::filesystem::path& assetsRoot,
		const std::filesystem::path& modelRelativePath,
		const LitMaterialConstants& materialConstants = {});

	void FillExtractedObjects(std::vector<ExtractedObject>& outObjects) const;

private:
	[[nodiscard]] Result<void> LoadTextureForSlot(
		class RenderResourceServices& resources,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& assetsRoot,
		const std::filesystem::path& texturePath,
		TextureHandle& outTexture);

	bool m_ready = false;
	LitMaterialConstants m_materialConstants{};
	std::optional<uint32_t> m_materialConstantsSlot{};
	MaterialHandle m_material{};
	std::vector<TextureHandle> m_materialTextures{};
	std::vector<FbxSceneInstance> m_instances{};
	FbxSdkContext m_fbxSdkContext{};
};
