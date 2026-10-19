#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/World/EntityPhysics.h"
#include "Engine/World/SceneDocument.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

class World;

[[nodiscard]] Result<std::string> WriteSceneDocument(const SceneDocument& document);
[[nodiscard]] Result<SceneDocument> ReadSceneDocument(std::string_view text);

// Resolves mesh keys, copies tint, and binds parentless bodies.
// A failure unbinds bodies from this call and destroys entities from this call.
[[nodiscard]] Result<std::vector<WorldDocumentBinding>> InstantiateScene(
	World& world,
	EntityPhysics& physics,
	const SceneDocument& document,
	std::span<const SceneMeshRef> meshes,
	const SceneRenderResources& render);
