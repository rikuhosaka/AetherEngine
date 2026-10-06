#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/Renderer/Texture/TextureTypes.h"
#include "Engine/World/World.h"

#include <filesystem>
#include <optional>

class FrameContext;
class Renderer;
class RHICommandList;

struct RoomDimensions
{
	float width = 10.0f;
	float depth = 10.0f;
	float height = 3.0f;
};

struct RoomMaterialConstants
{
	float tint[4] { 1.0f, 1.0f, 1.0f, 1.0f };
};

// Closed box room assembled from six unit Plane primitives (floor, four walls, ceiling).
// The floor sits at y = 0 and is centered on the origin in XZ.
class RoomSceneAssets
{
public:
	[[nodiscard]] bool IsReady() const noexcept { return m_ready; }

	[[nodiscard]] Result<void> EnsureInitialized(
		Renderer& renderer,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& shaderRoot,
		const RoomDimensions& dimensions = {},
		const RoomMaterialConstants& materialConstants = {});

	[[nodiscard]] Result<void> SpawnInto(World& world);

private:
	bool m_ready = false;
	bool m_spawned = false;
	MeshHandle m_planeMesh{};
	MaterialHandle m_material{};
	TextureHandle m_baseColor{};
	RoomMaterialConstants m_materialConstants{};
	std::optional<uint32_t> m_materialConstantsSlot{};
	RoomDimensions m_dimensions{};
};
