#pragma once

#include "Engine/Camera/CameraComponent.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Scene/EntityPhysics.h"
#include "Engine/Scene/LightComponent.h"
#include "Engine/World/Renderable.h"
#include "Engine/World/RigidBody.h"
#include "Engine/World/WorldDocument.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class World;

// Parallel to SceneDocument::world. A missing component means the entity does not have it.
struct SceneEntity
{
	uint32_t id = 0;
	std::string mesh{};
	float tint[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
	std::optional<RigidBody> body{};
	std::optional<CameraComponent> camera{};
	std::optional<LightComponent> light{};
};

struct SceneDocument
{
	WorldDocument world{};
	std::vector<SceneEntity> entities{};
};

struct SceneMeshRef
{
	std::string key{};
	WorldMeshId mesh{};
};

struct SceneRenderResources
{
	WorldMaterialId material{};
	WorldTextureId baseColor{};
	std::optional<uint32_t> materialConstantsSlot{};
};

[[nodiscard]] Result<std::string> WriteSceneDocument(const SceneDocument& document);
[[nodiscard]] Result<SceneDocument> ReadSceneDocument(std::string_view text);
[[nodiscard]] Result<SceneDocument> LoadSceneDocument(const std::filesystem::path& path);

// Binds RigidBody components. A failure unbinds bodies from this call.
[[nodiscard]] Result<EntityPhysics> BindRigidBodies(World& world);

// Resolves mesh keys, copies tint, and adds rigid body, camera, and light components.
// A failure destroys entities from this call.
[[nodiscard]] Result<std::vector<WorldDocumentBinding>> InstantiateScene(
	World& world,
	const SceneDocument& document,
	std::span<const SceneMeshRef> meshes,
	const SceneRenderResources& render);
