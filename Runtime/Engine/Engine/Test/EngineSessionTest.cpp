#include "Runtime/Engine/Engine/Test/EngineSessionTest.h"

#include "Runtime/Engine/Application/Subsystem/SubsystemContext.h"
#include "Runtime/Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Runtime/Engine/World/Component/CameraSystemComponent.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Engine/Engine/Subsystem/Engine.h"
#include "Runtime/Engine/Engine/Subsystem/InputSubsystem.h"
#include "Runtime/Engine/World/ExtractedObject.h"
#include "Runtime/Engine/World/DebugStatsServices.h"
#include "Runtime/Engine/World/Component/RigidBody.h"
#include "Runtime/Engine/World/World.h"
#include "Runtime/Engine/World/WorldExtract.h"

#include <cmath>
#include <vector>

namespace
{
[[nodiscard]] bool IsNearlyEqual(float left, float right)
{
	return std::fabs(left - right) <= 0.0001f;
}
} // namespace

Result<void> RunEngineSessionTests()
{
	SubsystemRegistry registry{};
	registry.Register(CreateInputSubsystem());
	SubsystemRegistry::SetCurrent(&registry);
	const struct CurrentRegistryScope
	{
		~CurrentRegistryScope() { SubsystemRegistry::SetCurrent(nullptr); }
	} clearCurrent;
	(void)clearCurrent;

	SubsystemContext context{};
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

	SceneContentServices* content = &engine.GetSceneContent();
	SessionServices* session = &engine.GetSession();
	WorldServices* published = &engine.GetWorldServices();
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
	if (!engine.GetWorld().Add<RigidBody>(body, RigidBody{}))
	{
		engine.Shutdown(context);
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Engine should accept a rigid body on the editor world");
	}
	content->loaded = true;

	session->requestMode = SessionMode::Paused;
	engine.Tick(context, 0.016f);
	if (session->requestMode != SessionMode::Editor || session->mode != SessionMode::Editor
		|| engine.GetWorld().IsPlaying() || published->generation != 1)
	{
		engine.Shutdown(context);
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"A paused request should not open a session");
	}

	session->mode = SessionMode::Playing;
	engine.Tick(context, 0.016f);
	if (session->mode != SessionMode::Editor || engine.GetWorld().IsPlaying())
	{
		engine.Shutdown(context);
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"The result mode should not open a session");
	}

	session->requestMode = SessionMode::Playing;
	engine.Tick(context, 0.016f);
	const EntityId camera = FindCamera(engine.GetWorld());
	const Transform* cameraTransform = engine.GetWorld().GetTransform(camera);
	if (!engine.GetWorld().IsPlaying() || session->mode != SessionMode::Playing
		|| published->world == editorWorld || editorWorld->IsPlaying()
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

	constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
	for (int stepIndex = 0; stepIndex < 30; ++stepIndex)
	{
		engine.FixedTick(context, kFixedDeltaSeconds);
	}
	DebugStatsServices stats{};
	std::vector<WorldExtractedObject> extracted{};
	Extract(engine.GetWorld(), extracted, 1.0f, stats);
	const float playedHeight = engine.GetWorld().GetTransform(body)->position.y;
	if (!(playedHeight < 3.5f))
	{
		engine.Shutdown(context);
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Playing should step the rigid body");
	}

	session->requestMode = SessionMode::Paused;
	engine.Tick(context, 0.016f);
	engine.FixedTick(context, kFixedDeltaSeconds);
	extracted.clear();
	Extract(engine.GetWorld(), extracted, 1.0f, stats);
	if (!engine.GetWorld().IsPlaying() || session->mode != SessionMode::Paused || published->generation != 2
		|| !IsNearlyEqual(engine.GetWorld().GetTransform(body)->position.y, playedHeight))
	{
		engine.Shutdown(context);
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Pause should keep the open play session and skip the step");
	}

	engine.GetWorld().GetTransform(body)->position.y = 1.0f;

	session->requestMode = SessionMode::Editor;
	engine.Tick(context, 0.016f);
	const Transform* restored = editorWorld->GetTransform(body);
	if (engine.GetWorld().IsPlaying() || session->mode != SessionMode::Editor
		|| published->world != editorWorld || published->generation != 3
		|| restored == nullptr || !IsNearlyEqual(restored->position.y, 4.0f))
	{
		engine.Shutdown(context);
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Stopping play should restore the editor document and advance the world generation");
	}

	engine.Shutdown(context);
	SubsystemRegistry::SetCurrent(nullptr);
	LOG_INFO(LogCategory::Core, "EngineSessionTests passed");
	return MakeOk();
}
