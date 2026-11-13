#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/SceneDocument/SceneDocument.h"
#include "Runtime/Renderer/Material/MaterialTypes.h"
#include "Runtime/Renderer/Mesh/MeshTypes.h"
#include "Runtime/AssetSystem/Texture/Loader/TextureLoadTypes.h"
#include "Runtime/Renderer/Texture/TextureTypes.h"
#include "Runtime/Engine/World/EntityId.h"
#include "Runtime/Engine/World/Transform.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class FbxSdkContext;
class FrameContext;
class RenderResourceServices;
class RHICommandList;
class World;

struct LitMaterialConstants
{
	float tint[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
};

class FbxSceneAssets final : public ISceneModelSpawn
{
public:
	FbxSceneAssets();
	~FbxSceneAssets() override;

	FbxSceneAssets(const FbxSceneAssets&) = delete;
	FbxSceneAssets& operator=(const FbxSceneAssets&) = delete;

	[[nodiscard]] bool IsReady() const noexcept { return m_ready; }

	[[nodiscard]] Result<void> EnsureInitialized(
		RenderResourceServices& resources,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& shaderRoot,
		const std::filesystem::path& assetsRoot,
		const std::filesystem::path& modelRelativePath,
		const LitMaterialConstants& materialConstants = {});

	// Parents the loaded node hierarchy under an existing entity. That entity's transform is the placement.
	[[nodiscard]] Result<void> SpawnUnder(World& world, EntityId parent) override;

private:
	struct FbxNodeSpawn
	{
		std::string name{};
		Transform local{};
		int32_t parentIndex = -1;
	};

	struct FbxSubmeshSpawn
	{
		std::string name{};
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
		TextureColorSpace colorSpace,
		const char* debugName,
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
	std::unique_ptr<FbxSdkContext> m_fbxSdkContext{};
};
