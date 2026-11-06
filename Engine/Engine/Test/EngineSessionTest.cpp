#include "Engine/Engine/Test/EngineSessionTest.h"

#include "Engine/Application/Services/InputServices.h"
#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Application/Services/SceneContentServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Camera/CameraSystemComponent.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Engine/Engine.h"
#include "Engine/World/World.h"

#include <cmath>

namespace
{
[[nodiscard]] bool IsNearlyEqual(float left, float right)
{
	return std::fabs(left - right) <= 0.0001f;
}
} // namespace

Result<void> RunEngineSessionTests()
{
	RenderServices render{};
	InputServices input{};

	SubsystemContext context{};
	context.RegisterService(&render);
	context.RegisterService(&input);

	Engine engine{ EngineConfig{} };
	const EntityId editorCamera = FindCamera(engine.GetWorld());
	Transform* editorCameraTransform = engine.GetWorld().GetTransform(editorCamera);
	if (!editorCamera.IsValid() || editorCameraTransform == nullptr)
	{
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Engine should spawn an editor camera");
	}
	editorCameraTransform->position = { 2.0f, 3.0f, 4.0f };
	if (auto initialized = engine.Initialize(context); !initialized)
	{
		return initialized;
	}

	SceneContentServices* content = context.GetService<SceneContentServices>();
	SessionServices* session = context.GetService<SessionServices>();
	WorldServices* published = context.GetService<WorldServices>();
	if (content == nullptr || session == nullptr || published == nullptr || published->world != &engine.GetWorld()
		|| published->generation != 1)
	{
		engine.Shutdown(context);
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Engine should publish the world before the first tick");
	}

	World* editorWorld = &engine.GetWorld();
	if (editorWorld->FindSubsystem("Game") != nullptr || editorWorld->FindSubsystem("Camera") != nullptr)
	{
		engine.Shutdown(context);
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"The editor world should not register play subsystems");
	}

	WorldSpawnDesc bodyDesc{};
	bodyDesc.name = "Body";
	bodyDesc.transform.position = { 0.0f, 4.0f, 0.0f };
	const EntityId body = engine.GetWorld().Spawn(std::move(bodyDesc));
	content->loaded = true;

	session->mode = SessionMode::Paused;
	engine.Tick(context, 0.016f);
	if (session->mode != SessionMode::Editor || engine.GetWorld().IsPlaying() || published->generation != 1)
	{
		engine.Shutdown(context);
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"A paused request should not open a session");
	}

	session->mode = SessionMode::Playing;
	engine.Tick(context, 0.016f);
	const EntityId camera = FindCamera(engine.GetWorld());
	const Transform* cameraTransform = engine.GetWorld().GetTransform(camera);
	if (!engine.GetWorld().IsPlaying() || published->world == editorWorld || editorWorld->IsPlaying()
		|| published->generation != 2 || camera == body || cameraTransform == nullptr
		|| !IsNearlyEqual(cameraTransform->position.x, 2.0f)
		|| !IsNearlyEqual(cameraTransform->position.y, 3.0f)
		|| !IsNearlyEqual(cameraTransform->position.z, 4.0f))
	{
		engine.Shutdown(context);
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Play should copy the editor world, start from the editor camera pose, and advance the world generation");
	}

	session->mode = SessionMode::Paused;
	engine.Tick(context, 0.016f);
	engine.GetWorld().GetTransform(body)->position.y = 1.0f;
	if (!engine.GetWorld().IsPlaying() || published->generation != 2)
	{
		engine.Shutdown(context);
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Pause should keep the open play session");
	}

	session->mode = SessionMode::Editor;
	engine.Tick(context, 0.016f);
	const Transform* restored = editorWorld->GetTransform(body);
	if (engine.GetWorld().IsPlaying() || published->world != editorWorld || published->generation != 3
		|| restored == nullptr || !IsNearlyEqual(restored->position.y, 4.0f))
	{
		engine.Shutdown(context);
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Stopping play should restore the editor document and advance the world generation");
	}

	engine.Shutdown(context);
	LOG_INFO(LogCategory::Core, "EngineSessionTests passed");
	return MakeOk();
}
