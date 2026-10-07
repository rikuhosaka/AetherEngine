#pragma once

#include "Engine/Renderer/Scene/RenderPlaceholders.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"

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

	void Extract(std::span<const ExtractedObject> objects, uint32_t frameIndex);
	void ExtractView(const ExtractedView& view);
	void ExtractLighting(const ExtractedLighting& lighting);

	void Build(
		FrameContext& frameContext,
		RHICommandList* commandList,
		RenderResourceServices& resources);

	[[nodiscard]] const ExtractedFrame& GetExtractedFrame() const noexcept { return m_extractedFrame; }
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
	ExtractedFrame m_extractedFrame{};
	ExtractedView m_extractedView{};
	ExtractedLighting m_extractedLighting{};
	bool m_hasExtractedView = false;
	bool m_hasExtractedLighting = false;
	RenderFrameSnapshot m_snapshot{};
	RenderPlaceholders m_placeholders{};
	std::unordered_map<RenderObjectId, MaterialInstanceCacheEntry, RenderObjectIdHash> m_instanceCache{};
};
