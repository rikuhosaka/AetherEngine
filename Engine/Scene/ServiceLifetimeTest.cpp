#include "Engine/Scene/ServiceLifetimeTest.h"

#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Scene/EditorScene.h"
#include "Engine/World/World.h"

Result<void> RunServiceLifetimeTests()
{
	World editor{};
	World play{};
	WorldServices services{};
	services.Publish(&editor);
	if (services.world != &editor || services.generation != 1)
	{
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Publishing a world should store it and start the generation at 1");
	}

	const uint32_t generation = services.generation;
	services.Publish(&editor);
	if (services.world != &editor || services.generation != generation)
	{
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Publishing the same world should keep the generation");
	}

	services.Publish(&play);
	if (services.world != &play || services.generation != generation + 1)
	{
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Publishing a different world should advance the generation");
	}

	EditorScene scene{};
	scene.SetDocument(SceneDocument{});
	WorldDocumentBinding binding{};
	binding.id = 7;
	binding.entity = EntityId{ 3, 1 };
	scene.SetBindings({ binding });

	EditorSceneServices published{};
	scene.Publish(published);
	scene.SetBindings({});
	if (published.bindings.size() != 1 || published.bindings[0].id != binding.id
		|| published.bindings[0].entity != binding.entity)
	{
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Published bindings should outlive the editor scene's binding list");
	}

	LOG_INFO(LogCategory::Core, "ServiceLifetimeTests passed");
	return MakeOk();
}
