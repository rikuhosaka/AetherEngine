#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/Renderer/Texture/TextureTypes.h"
#include "Engine/World/EntityPhysics.h"

#include <filesystem>
#include <optional>

class EditorScene;
class FrameContext;
class RenderResourceServices;
class RHICommandList;

// Mesh floor and static box, with a dynamic sphere, capsule, and convex hull. Render meshes match the physics shapes.
class PhysicsDropScene
{
public:
	[[nodiscard]] Result<void> EnsureInitialized(
		EditorScene& scene,
		RenderResourceServices& resources,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& shaderRoot,
		const std::filesystem::path& scenePath);

	[[nodiscard]] Result<void> SpawnInto(EditorScene& scene);

	// Binds the editor document's bodies onto a cloned play world. The caller owns the result.
	[[nodiscard]] Result<EntityPhysics> CreatePlayPhysics(const EditorScene& editor, World& playWorld);

private:
	bool m_ready = false;
	bool m_spawned = false;
	MeshHandle m_floorMesh{};
	MeshHandle m_boxMesh{};
	MeshHandle m_sphereMesh{};
	MeshHandle m_capsuleMesh{};
	MeshHandle m_hullMesh{};
	MaterialHandle m_material{};
	TextureHandle m_baseColor{};
	std::optional<uint32_t> m_materialConstantsSlot{};
	EntityPhysics m_physics{};
};
