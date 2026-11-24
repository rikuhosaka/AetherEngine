#pragma once

#include "Runtime/Renderer/Scene/RenderPlaceholders.h"
#include "Runtime/Renderer/Scene/RenderSceneTypes.h"

#include <filesystem>
#include <span>
#include <unordered_map>
#include <vector>

class FrameContext;
class MaterialSystemServices;
class RenderResourceServices;
class RHICommandList;

class RenderScene
{
public:
	void SetShaderRoot(const std::filesystem::path& shaderRoot) { m_shaderRoot = shaderRoot; }

	void BeginFrame();

	void Build(
		const RenderFrame& frame,
		uint32_t frameIndex,
		FrameContext& frameContext,
		RHICommandList* commandList,
		RenderResourceServices& resources);

	[[nodiscard]] const RenderFrameSnapshot& GetSnapshot() const noexcept { return m_snapshot; }

private:
	struct MaterialInstanceCacheEntry
	{
		MaterialHandle material{};
		StoredMaterialParameterOverrides overrides{};
		std::vector<TextureHandle> resolvedTextures{};
		std::vector<std::vector<std::byte>> constantBuffers{};
		MaterialInstanceHandle instance{};
		uint32_t lastUsedFrame = 0;
	};

	struct RenderObjectIdHash
	{
		size_t operator()(RenderObjectId id) const noexcept
		{
			return (static_cast<size_t>(id.Generation) << 32) | static_cast<size_t>(id.Index);
		}
	};

	[[nodiscard]] MaterialInstanceHandle ResolveMaterialInstance(
		RenderObjectId objectId,
		MaterialHandle material,
		const StoredMaterialParameterOverrides& overrides,
		std::span<const TextureHandle> resolvedTextures,
		std::span<const std::vector<std::byte>> constantBuffers,
		RenderResourceServices& resources,
		const RenderPlaceholderResources& placeholders,
		uint32_t frameIndex);

	void ReleaseStaleMaterialInstances(MaterialSystemServices& materialServices, uint32_t frameIndex);

	std::filesystem::path m_shaderRoot{};
	RenderFrameSnapshot m_snapshot{};
	RenderPlaceholders m_placeholders{};
	std::unordered_map<RenderObjectId, MaterialInstanceCacheEntry, RenderObjectIdHash> m_instanceCache{};
};
