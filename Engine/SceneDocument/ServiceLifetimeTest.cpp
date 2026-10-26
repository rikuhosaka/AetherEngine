#include "Engine/SceneDocument/ServiceLifetimeTest.h"

#include "Engine/Application/Services/EditorSceneServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/SceneDocument/SceneDocumentSubsystem.h"
#include "Engine/World/World.h"

#include <filesystem>

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

	SceneDocumentSubsystem scene{};
	scene.SetDocument(SceneDocument{});
	scene.SetPath("Scenes/PhysicsDrop.json");

	EditorSceneServices published{};
	scene.Publish(published);
	scene.SetPath("Scenes/Other.json");
	if (published.document != &scene.GetDocument()
		|| published.path != std::filesystem::path("Scenes/PhysicsDrop.json"))
	{
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Published scene path should outlive a later path change");
	}

	LOG_INFO(LogCategory::Core, "ServiceLifetimeTests passed");
	return MakeOk();
}
