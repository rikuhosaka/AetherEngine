#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Physics/EntityPhysics.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/Renderer/Texture/TextureTypes.h"
#include "Game/SceneDocument.h"

#include <filesystem>
#include <optional>
#include <vector>

class FrameContext;
class RenderResourceServices;
class RHICommandList;

// Mesh floor and static box, with a dynamic sphere, capsule, and convex hull. Render meshes match the physics shapes.
class PhysicsDropScene
{
public:
	[[nodiscard]] Result<void> EnsureInitialized(
		RenderResourceServices& resources,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const std::filesystem::path& shaderRoot,
		const std::filesystem::path& scenePath);

	[[nodiscard]] Result<void> SpawnInto(World& world);

	// Writes the editor world. The play clone is not read.
	[[nodiscard]] Result<std::string> CaptureEditor(const World& world) const;

	[[nodiscard]] Result<void> BeginPlay(World& playWorld);
	void EndPlay();

	[[nodiscard]] Result<void> Step(World& world, float fixedDeltaSeconds);

	void WriteRenderTransforms(World& world, float alpha);

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
	SceneDocument m_document{};
	std::vector<WorldDocumentBinding> m_bindings{};
	EntityPhysics m_physics{};
	std::optional<EntityPhysics> m_playPhysics{};
};
