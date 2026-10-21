#include "Engine/World/WorldDocument.h"

#include "Engine/World/World.h"

#include <utility>
#include <vector>

namespace
{
constexpr int kMaxParentDepth = 64;

[[nodiscard]] const WorldDocumentEntity* FindEntity(const WorldDocument& document, uint32_t id)
{
	for (const WorldDocumentEntity& entity : document)
	{
		if (entity.id == id)
		{
			return &entity;
		}
	}

	return nullptr;
}

[[nodiscard]] const WorldDocumentBinding* FindBinding(
	const std::vector<WorldDocumentBinding>& bindings,
	uint32_t id)
{
	for (const WorldDocumentBinding& binding : bindings)
	{
		if (binding.id == id)
		{
			return &binding;
		}
	}

	return nullptr;
}

void DestroySpawned(World& world, const std::vector<EntityId>& spawned)
{
	for (const EntityId id : spawned)
	{
		world.Destroy(id);
	}
}

[[nodiscard]] Result<void> Validate(const WorldDocument& document)
{
	for (size_t index = 0; index < document.size(); ++index)
	{
		const WorldDocumentEntity& entity = document[index];
		if (entity.id == 0)
		{
			return FailRuntime(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"World document id must be non-zero");
		}

		for (size_t earlier = 0; earlier < index; ++earlier)
		{
			if (document[earlier].id == entity.id)
			{
				return FailRuntime(
					LogCategory::ECS,
					ErrorCode::InvalidArgument,
					"World document id is duplicated");
			}
		}
	}

	for (const WorldDocumentEntity& entity : document)
	{
		if (entity.parent == 0)
		{
			continue;
		}

		if (FindEntity(document, entity.parent) == nullptr)
		{
			return FailRuntime(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"World document parent is missing");
		}

		uint32_t current = entity.parent;
		bool reachedRoot = false;
		for (int depth = 0; depth < kMaxParentDepth; ++depth)
		{
			if (current == entity.id)
			{
				return FailRuntime(
					LogCategory::ECS,
					ErrorCode::InvalidArgument,
					"World document parent chain cycles");
			}

			const WorldDocumentEntity* node = FindEntity(document, current);
			if (node == nullptr || node->parent == 0)
			{
				reachedRoot = true;
				break;
			}

			current = node->parent;
		}

		if (!reachedRoot)
		{
			return FailRuntime(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"World document parent chain cycles or is deeper than 64");
		}
	}

	return MakeOk();
}
} // namespace

Result<std::vector<WorldDocumentBinding>> Instantiate(World& world, const WorldDocument& document)
{
	if (auto validation = Validate(document); !validation)
	{
		return MakeFail<std::vector<WorldDocumentBinding>>(validation.error.code, validation.error.message);
	}

	std::vector<WorldDocumentBinding> bindings{};
	std::vector<EntityId> spawned{};
	bindings.reserve(document.size());
	spawned.reserve(document.size());

	for (const WorldDocumentEntity& entity : document)
	{
		WorldSpawnDesc desc{};
		desc.name = entity.name;
		desc.transform = entity.transform;

		const EntityId spawnedId = world.Spawn(std::move(desc));
		spawned.push_back(spawnedId);
		bindings.push_back(WorldDocumentBinding{ entity.id, spawnedId });

		if (!entity.hasRenderable)
		{
			continue;
		}

		Renderable renderable{};
		renderable.submeshIndex = entity.submeshIndex;
		renderable.layerMask = entity.layerMask;
		renderable.visible = entity.visible;
		if (!world.Add<Renderable>(spawnedId, std::move(renderable)))
		{
			DestroySpawned(world, spawned);
			return FailRuntime<std::vector<WorldDocumentBinding>>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"World document renderable was rejected");
		}
	}

	for (const WorldDocumentEntity& entity : document)
	{
		if (entity.parent == 0)
		{
			continue;
		}

		const WorldDocumentBinding* child = FindBinding(bindings, entity.id);
		const WorldDocumentBinding* parent = FindBinding(bindings, entity.parent);
		if (child == nullptr || parent == nullptr || !world.SetParent(child->entity, parent->entity))
		{
			DestroySpawned(world, spawned);
			return FailRuntime<std::vector<WorldDocumentBinding>>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"World document parent link was rejected");
		}
	}

	return MakeOk(std::move(bindings));
}
