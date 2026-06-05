#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/Renderer/Texture/TextureTypes.h"

#include <filesystem>

class FrameContext;
class Renderer;
class RHICommandList;

struct QuadSceneConstants
{
	float mvp[16]{};
};

struct QuadMaterialConstants
{
	float tint[4]{ 0.80f, 0.2f, 0.0f, 1.0f };
};

class QuadSceneAssets
{
public:
	[[nodiscard]] bool IsReady() const noexcept { return m_ready; }

	[[nodiscard]] MeshHandle GetMesh() const noexcept { return m_mesh; }
	[[nodiscard]] MaterialHandle GetMaterial() const noexcept { return m_material; }
	[[nodiscard]] TextureHandle GetWhiteTexture() const noexcept { return m_whiteTexture; }
	[[nodiscard]] const QuadSceneConstants& GetSceneConstants() const noexcept { return m_sceneConstants; }
	[[nodiscard]] const QuadMaterialConstants& GetMaterialConstants() const noexcept
	{
		return m_materialConstants;
	}

	[[nodiscard]] Result<void> EnsureInitialized(
		Renderer& renderer,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& shaderRoot,
		const QuadMaterialConstants& color = {});

	void FillExtractedObject(ExtractedObject& object) const;

private:
	bool m_ready = false;
	MeshHandle m_mesh{};
	MaterialHandle m_material{};
	TextureHandle m_whiteTexture{};
	QuadSceneConstants m_sceneConstants{};
	QuadMaterialConstants m_materialConstants{};
};
