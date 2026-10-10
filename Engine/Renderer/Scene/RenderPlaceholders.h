#pragma once

#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/Renderer/Texture/TextureTypes.h"

#include <filesystem>

class FrameContext;
class RenderResourceServices;
class RHICommandList;

struct RenderPlaceholderResources
{
	MeshHandle mesh{};
	MaterialHandle material{};
	TextureHandle texture{};
	TextureHandle normalTexture{};
	MaterialInstanceHandle materialInstance{};
};

class RenderPlaceholders
{
public:
	[[nodiscard]] bool EnsureInitialized(
		RenderResourceServices& resources,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& shaderRoot,
		std::string* outError = nullptr);

	[[nodiscard]] const RenderPlaceholderResources& Get() const noexcept { return m_resources; }
	[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
	RenderPlaceholderResources m_resources{};
	bool m_initialized = false;
};
