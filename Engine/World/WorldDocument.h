#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/World/EntityId.h"
#include "Engine/World/Renderable.h"
#include "Engine/World/Transform.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// id is 1 or greater. parent 0 means the entity is a root.
struct WorldDocumentEntity
{
	uint32_t id = 0;
	uint32_t parent = 0;
	std::string name{};
	Transform transform{};
	uint32_t submeshIndex = 0;
	uint8_t layerMask = WorldLayer::Opaque;
	bool visible = true;
	bool hasRenderable = true;
};

using WorldDocument = std::vector<WorldDocumentEntity>;

inline constexpr int kWorldDocumentVersion = 1;

struct WorldDocumentBinding
{
	uint32_t id = 0;
	EntityId entity{};
};

class World;

// Spawns every entity without a parent, then links parents from the document ids.
// Mesh, material, and texture handles stay invalid. A rejected document adds nothing.
[[nodiscard]] Result<std::vector<WorldDocumentBinding>> Instantiate(
	World& world,
	const WorldDocument& document);

// Versioned document text. A root parent is null. File paths stay with the caller.
[[nodiscard]] Result<std::string> WriteWorldDocument(const WorldDocument& document);
[[nodiscard]] Result<WorldDocument> ReadWorldDocument(std::string_view text);
