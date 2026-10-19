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

// Binds document bodies onto an existing world. Bindings without a body are skipped.
// A failure unbinds bodies from this call.
[[nodiscard]] Result<EntityPhysics> BindScenePhysics(
	World& world,
	const SceneDocument& document,
	std::span<const WorldDocumentBinding> bindings);

// Resolves mesh keys, copies tint, and binds parentless bodies.
// A failure unbinds bodies from this call and destroys entities from this call.
[[nodiscard]] Result<std::vector<WorldDocumentBinding>> InstantiateScene(
	World& world,
	EntityPhysics& physics,
	const SceneDocument& document,
	std::span<const SceneMeshRef> meshes,
	const SceneRenderResources& render);
