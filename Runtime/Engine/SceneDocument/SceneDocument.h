#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/SceneDocument/SceneComponents.h"
#include "Runtime/Engine/World/WorldDocument.h"

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
	std::string model{};
	float tint[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
	SceneTransform transform{};
	std::optional<SceneRigidBody> body{};
	std::optional<SceneCamera> camera{};
	std::optional<SceneLight> light{};
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

// Resolves mesh keys, copies tint, and adds rigid body, camera, and light components.
// A failure destroys entities from this call.
[[nodiscard]] Result<std::vector<WorldDocumentBinding>> InstantiateScene(
	World& world,
	const SceneDocument& document,
	std::span<const SceneMeshRef> meshes,
	const SceneRenderResources& render);
