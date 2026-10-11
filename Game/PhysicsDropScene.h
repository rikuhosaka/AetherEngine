#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/Renderer/Texture/TextureTypes.h"
#include "Engine/World/World.h"

#include <filesystem>
#include <optional>

class FrameContext;
class RenderResourceServices;
class RHICommandList;

// Static box with a dynamic sphere above it. Render meshes match the physics shapes.
class PhysicsDropScene
{
public:
	[[nodiscard]] Result<void> EnsureInitialized(
		RenderResourceServices& resources,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& shaderRoot);

	[[nodiscard]] Result<void> SpawnInto(World& world);

	[[nodiscard]] Result<void> Step(float fixedDeltaSeconds);

	void WriteRenderTransforms(World& world, float alpha);

private:
	struct BodyBinding
	{
		EntityId entity{};
		PhysicsBodyId body{};
	};

	struct MaterialTint
	{
		float tint[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	[[nodiscard]] WorldSpawnDesc MakeSpawnDesc(
		const Transform& transform,
		MeshHandle mesh,
		const MaterialTint& tint) const;

	bool m_ready = false;
	bool m_spawned = false;
	MeshHandle m_boxMesh{};
	MeshHandle m_sphereMesh{};
	MaterialHandle m_material{};
	TextureHandle m_baseColor{};
	std::optional<uint32_t> m_materialConstantsSlot{};
	PhysicsWorld m_physics{};
	BodyBinding m_box{};
	BodyBinding m_sphere{};
};
