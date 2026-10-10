#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/Renderer/Model/Loader/FbxSdkContext.h"
#include "Engine/Renderer/Texture/TextureTypes.h"
#include "Engine/World/World.h"

#include <filesystem>
#include <optional>
#include <vector>

class FrameContext;
class RenderResourceServices;
class RHICommandList;

struct LitMaterialConstants
{
	float tint[4] { 1.0f, 1.0f, 1.0f, 1.0f };
};

class FbxSceneAssets
{
public:
	[[nodiscard]] bool IsReady() const noexcept { return m_ready; }

	[[nodiscard]] Result<void> EnsureInitialized(
		RenderResourceServices& resources,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& shaderRoot,
		const std::filesystem::path& assetsRoot,
		const std::filesystem::path& modelRelativePath,
		const LitMaterialConstants& materialConstants = {});

	[[nodiscard]] Result<void> SpawnInto(World& world);

private:
	struct FbxNodeSpawn
	{
		Transform local{};
		int32_t parentIndex = -1;
	};

	struct FbxSubmeshSpawn
	{
		uint32_t nodeIndex = 0;
		MeshHandle mesh{};
		TextureHandle baseColor{};
		TextureHandle normal{};
		uint32_t submeshIndex = 0;
		float worldMatrix[16]{};
	};

	[[nodiscard]] Result<void> LoadTextureForSlot(
		RenderResourceServices& resources,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& assetsRoot,
		const std::filesystem::path& texturePath,
		TextureHandle& outTexture);

	bool m_ready = false;
	bool m_spawned = false;
	LitMaterialConstants m_materialConstants{};
	std::optional<uint32_t> m_materialConstantsSlot{};
	MaterialHandle m_material{};
	std::vector<TextureHandle> m_materialTextures{};
	std::vector<TextureHandle> m_normalTextures{};
	std::vector<FbxNodeSpawn> m_nodes{};
	std::vector<FbxSubmeshSpawn> m_submeshes{};
	FbxSdkContext m_fbxSdkContext{};
};
