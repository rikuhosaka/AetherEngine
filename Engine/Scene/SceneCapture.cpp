#include "Engine/Scene/SceneCapture.h"

#include "Engine/World/World.h"

#include <string>
#include <utility>
#include <vector>

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

[[nodiscard]] const WorldDocumentBinding* FindBindingByEntity(
	const std::vector<WorldDocumentBinding>& bindings,
	EntityId entity)
{
	for (const WorldDocumentBinding& binding : bindings)
	{
		if (binding.entity == entity)
		{
			return &binding;
		}
	}

	return nullptr;
}
} // namespace

Result<SceneDocument> CaptureScene(
	const World& world,
	const SceneDocument& authored,
	std::span<const WorldDocumentBinding> bindings)
{
	if (!SameIds(authored))
	{
		return FailRuntime<SceneDocument>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document entities must match the world document");
	}

	const std::vector<WorldDocumentBinding> bindingList(bindings.begin(), bindings.end());
	SceneDocument captured{};
	captured.world.reserve(authored.world.size());
	captured.entities.reserve(authored.entities.size());

	for (size_t index = 0; index < authored.world.size(); ++index)
	{
		const WorldDocumentEntity& authoredWorld = authored.world[index];
		const WorldDocumentBinding* binding = FindBinding(bindingList, authoredWorld.id);
		if (binding == nullptr || !world.IsAlive(binding->entity))
		{
			return FailRuntime<SceneDocument>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene capture requires a live editor entity");
		}

		const Transform* transform = world.GetTransform(binding->entity);
		const Renderable* renderable = world.Get<Renderable>(binding->entity);
		if (transform == nullptr || renderable == nullptr)
		{
			return FailRuntime<SceneDocument>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene capture requires a live editor entity");
		}

		WorldDocumentEntity capturedWorld = authoredWorld;
		capturedWorld.name = std::string(world.GetName(binding->entity));
		capturedWorld.transform = *transform;
		capturedWorld.submeshIndex = renderable->submeshIndex;
		capturedWorld.layerMask = renderable->layerMask;
		capturedWorld.visible = renderable->visible;

		const EntityId parent = world.GetParent(binding->entity);
		if (!parent.IsValid())
		{
			capturedWorld.parent = 0;
		}
		else
		{
			const WorldDocumentBinding* parentBinding = FindBindingByEntity(bindingList, parent);
			if (parentBinding == nullptr)
			{
				return FailRuntime<SceneDocument>(
					LogCategory::ECS,
					ErrorCode::InvalidArgument,
					"Scene capture parent is missing from the document");
			}

			capturedWorld.parent = parentBinding->id;
		}

		SceneEntity capturedEntity = authored.entities[index];
		if (const RigidBody* body = world.Get<RigidBody>(binding->entity))
		{
			capturedEntity.body = *body;
		}
		else
		{
			capturedEntity.body.reset();
		}

		if (const CameraComponent* camera = world.Get<CameraComponent>(binding->entity))
		{
			capturedEntity.camera = *camera;
		}
		else
		{
			capturedEntity.camera.reset();
		}

		if (const LightComponent* light = world.Get<LightComponent>(binding->entity))
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
