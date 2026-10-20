#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/World/EntityPhysics.h"
#include "Engine/World/SceneDocument.h"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class World;

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
