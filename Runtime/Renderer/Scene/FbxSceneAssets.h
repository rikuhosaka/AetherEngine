#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/Material/MaterialTypes.h"
#include "Runtime/Renderer/Mesh/MeshTypes.h"
#include "Runtime/Renderer/Scene/SceneGpuUpload.h"
#include "Runtime/Renderer/Texture/TextureTypes.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

class FrameContext;
class RenderResourceServices;
class RHICommandList;

struct LitMaterialConstants
{
	float tint[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
	float shininess = 32.0f;
	float pad[3]{};
};

static_assert(sizeof(LitMaterialConstants) == 32);
static_assert(offsetof(LitMaterialConstants, shininess) == 16);

struct FbxNodeRecord
{
	std::string name{};
	float localTransform[16]{};
	int32_t parentIndex = -1;
};

struct FbxSubmeshRecord
{
	std::string name{};
	uint32_t nodeIndex = 0;
	MeshHandle mesh{};
	TextureHandle baseColor{};
	TextureHandle normal{};
	TextureHandle specular{};
	uint32_t submeshIndex = 0;
	float worldMatrix[16]{};
};

class FbxSceneAssets final
{
public:
	FbxSceneAssets();
	~FbxSceneAssets();

	FbxSceneAssets(const FbxSceneAssets&) = delete;
	FbxSceneAssets& operator=(const FbxSceneAssets&) = delete;

	[[nodiscard]] bool IsReady() const noexcept { return m_ready; }
	[[nodiscard]] const std::vector<FbxNodeRecord>& Nodes() const noexcept { return m_nodes; }
	[[nodiscard]] const std::vector<FbxSubmeshRecord>& Submeshes() const noexcept { return m_submeshes; }
	[[nodiscard]] MaterialHandle GetMaterial() const noexcept { return m_material; }
	[[nodiscard]] const LitMaterialConstants& MaterialConstants() const noexcept { return m_materialConstants; }
	[[nodiscard]] std::optional<uint32_t> MaterialConstantsSlot() const noexcept { return m_materialConstantsSlot; }

	[[nodiscard]] Result<void> EnsureInitialized(
		RenderResourceServices& resources,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& shaderRoot,
		const SceneModelUpload& model,
		std::span<const TextureHandle> textures,
		const LitMaterialConstants& materialConstants = {});

private:
	bool m_ready = false;
	LitMaterialConstants m_materialConstants{};
	std::optional<uint32_t> m_materialConstantsSlot{};
	MaterialHandle m_material{};
	std::vector<TextureHandle> m_materialTextures{};
	std::vector<TextureHandle> m_normalTextures{};
	std::vector<TextureHandle> m_specularTextures{};
	std::vector<FbxNodeRecord> m_nodes{};
	std::vector<FbxSubmeshRecord> m_submeshes{};
};
