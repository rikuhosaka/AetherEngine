#include "Engine/SceneDocument/SceneCapture.h"

#include "Engine/World/DocumentEntityId.h"
#include "Engine/World/World.h"

#include <string>
#include <utility>

namespace
{
[[nodiscard]] bool SameIds(const SceneDocument& document)
{
	if (document.world.size() != document.entities.size())
	{
		return false;
	}

	for (size_t index = 0; index < document.world.size(); ++index)
	{
		if (document.world[index].id != document.entities[index].id)
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] EntityId FindDocumentEntity(const World& world, uint32_t id)
{
	EntityId found{};
	world.ForEach<DocumentEntityId>([&found, id](EntityId entity, const DocumentEntityId& component) {
		if (!found.IsValid() && component.id == id)
		{
			found = entity;
		}
	});
	return found;
}
} // namespace

Result<SceneDocument> CaptureScene(const World& world, const SceneDocument& authored)
{
	if (!SameIds(authored))
	{
		return FailRuntime<SceneDocument>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document entities must match the world document");
	}

	SceneDocument captured{};
	captured.world.reserve(authored.world.size());
	captured.entities.reserve(authored.entities.size());

	for (size_t index = 0; index < authored.world.size(); ++index)
	{
		const WorldDocumentEntity& authoredWorld = authored.world[index];
		const EntityId entity = FindDocumentEntity(world, authoredWorld.id);
		if (!world.IsAlive(entity))
		{
			return FailRuntime<SceneDocument>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene capture requires a live editor entity");
		}

		const bool model = !authored.entities[index].model.empty();
		const Transform* transform = world.GetTransform(entity);
		const Renderable* renderable = world.Get<Renderable>(entity);
		if (transform == nullptr || (!model && renderable == nullptr) || (model && renderable != nullptr))
		{
			return FailRuntime<SceneDocument>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene capture requires a live editor entity");
		}

		WorldDocumentEntity capturedWorld = authoredWorld;
		capturedWorld.name = std::string(world.GetName(entity));
		capturedWorld.transform = *transform;
		capturedWorld.hasRenderable = !model;
		if (!model)
		{
			capturedWorld.submeshIndex = renderable->submeshIndex;
			capturedWorld.layerMask = renderable->layerMask;
			capturedWorld.visible = renderable->visible;
		}

		const EntityId parent = world.GetParent(entity);
		if (!parent.IsValid())
		{
			capturedWorld.parent = 0;
		}
		else
		{
			const DocumentEntityId* parentId = world.Get<DocumentEntityId>(parent);
			if (parentId == nullptr)
			{
				return FailRuntime<SceneDocument>(
					LogCategory::ECS,
					ErrorCode::InvalidArgument,
					"Scene capture parent is missing from the document");
			}

			capturedWorld.parent = parentId->id;
		}

		SceneEntity capturedEntity = authored.entities[index];
		if (const RigidBody* body = world.Get<RigidBody>(entity))
		{
			capturedEntity.body = *body;
		}
		else
		{
			capturedEntity.body.reset();
		}

		if (const CameraComponent* camera = world.Get<CameraComponent>(entity))
		{
			capturedEntity.camera = *camera;
		}
		else
		{
			capturedEntity.camera.reset();
		}

		if (const LightComponent* light = world.Get<LightComponent>(entity))
		{
			capturedEntity.light = *light;
		}
		else
		{
			capturedEntity.light.reset();
		}

		captured.world.push_back(std::move(capturedWorld));
		captured.entities.push_back(std::move(capturedEntity));
	}

	return MakeOk(std::move(captured));
}
