#include "Engine/Engine/Engine.h"

#include "Editor/Scene/SceneLoaderSubsystem.h"
#include "Editor/UI/EditorUiSubsystem.h"
#include "Engine/Application/Services/EditorCameraServices.h"
#include "Engine/Application/Services/InputServices.h"
#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Core/Log/LogMacros.h"

#include <memory>
#include <utility>

namespace
{
constexpr const char* kDependencies[] = { "Input" };

// Starts or stops play from the session the editor UI wrote.
// A paused request does not open a session. Returns whether the session changed.
[[nodiscard]] bool ApplyPlaySession(
	World& world,
	SessionServices& session,
	bool contentLoaded,
	const DirectX::XMFLOAT3* cameraPosition,
	const DirectX::XMFLOAT3* cameraForward)
{
	const bool sessionOpen = world.IsPlaying();
	const bool wantSession = session.mode == SessionMode::Playing || session.mode == SessionMode::Paused;
	if (wantSession && !sessionOpen)
	{
		if (session.mode == SessionMode::Paused)
		{
			session.mode = SessionMode::Editor;
			return false;
		}

		if (auto begun = world.BeginPlay(contentLoaded, cameraPosition, cameraForward); !begun)
		{
			LogResult(begun, LogCategory::Core);
			session.mode = SessionMode::Editor;
			return false;
		}

		return true;
	}

	if (!wantSession && sessionOpen)
	{
		world.EndPlay();
		return true;
	}

	return false;
}
} // namespace

Engine::Engine(EngineConfig config)
	: m_config(std::move(config))
{
}

void Engine::RegisterSubsystems(SubsystemRegistry& registry) &&
{
	registry.Register(std::make_unique<Engine>(std::move(*this)));
	registry.Register(CreateEditorUiSubsystem());
	registry.Register(CreateEditorSubsystem());
}

const EngineConfig& Engine::GetConfig() const
{
	return m_config;
}

World& Engine::GetWorld()
{
	return m_world;
}

const World& Engine::GetWorld() const
{
	return m_world;
}

std::span<const char* const> Engine::GetDependencies() const
{
	return kDependencies;
}

Result<void> Engine::Initialize(SubsystemContext& ctx)
{
	if (ctx.GetService<RenderServices>() == nullptr || ctx.GetService<InputServices>() == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"Engine requires RenderServices and InputServices");
	}

	m_session.mode = SessionMode::Editor;
	m_worldServices.Publish(&m_world);
	ctx.RegisterService(&m_debugStats);
	ctx.RegisterService(&m_worldServices);
	ctx.RegisterService(&m_session);
	ctx.RegisterService(&m_content);
	return MakeOk();
}

void Engine::Tick(SubsystemContext& ctx, float deltaSeconds)
{
	InputServices* inputServices = ctx.GetService<InputServices>();
	const EditorCameraServices* camera = ctx.GetService<EditorCameraServices>();
	const bool cameraValid = camera != nullptr && camera->valid;
	const DirectX::XMFLOAT3* cameraPosition = cameraValid ? &camera->position : nullptr;
	const DirectX::XMFLOAT3* cameraForward = cameraValid ? &camera->forward : nullptr;
	if (ApplyPlaySession(m_world, m_session, m_content.loaded, cameraPosition, cameraForward))
	{
		m_worldServices.Advance();
	}

	const bool relativeMouse = m_world.Tick(
		deltaSeconds,
		m_session,
		m_worldServices,
		inputServices != nullptr ? inputServices->state : nullptr,
		inputServices != nullptr && inputServices->mouseCaptured,
		inputServices != nullptr && inputServices->keyboardCaptured);
	if (m_world.IsPlaying() && inputServices != nullptr)
	{
		inputServices->relativeMouse = relativeMouse;
	}
}

void Engine::FixedTick(SubsystemContext& /*ctx*/, float fixedDeltaSeconds)
{
	m_world.FixedTick(m_session, fixedDeltaSeconds);
}

void Engine::Shutdown(SubsystemContext& ctx)
{
	m_world.EndPlay();

	ctx.UnregisterService(&m_content);
	m_content = {};
	ctx.UnregisterService(&m_session);
	m_session = {};
	ctx.UnregisterService(&m_worldServices);
	m_worldServices = {};
	ctx.UnregisterService(&m_debugStats);
	m_debugStats = {};
}
