#include "Engine/Engine/Engine.h"

#include "Editor/UI/EditorUiSubsystem.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Platform/InputSubsystem.h"
#include "Engine/Camera/CameraSystemComponent.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/World/GameSubsystem.h"

#include <memory>
#include <utility>

namespace
{
constexpr const char* kDependencies[] = { "Input" };

// UI が書いた SessionMode を、編集用 World とゲーム用 World の切り替えへ反映する。
// ボタンはモードを書くだけで BeginPlay / EndPlay は呼ばない。反映はこの関数だけが行う。
// セッションを開くのは Playing への遷移だけ。最初から Paused を書かれてもセッションは開かず Editor に戻す。
// 開くときは編集用 World の文書をコピーし、Game を足してから BeginPlay する。
// カメラアクターは文書に入っているのでコピーが視点になる。無ければ BeginPlay が作る。
// 開いているセッションの一時停止は World を破棄しない。閉じるのは Editor へ戻ったとき。
// 公開先のポインタは続く Tick が書き換える。
void ApplyPlaySession(
	World& editorWorld,
	std::unique_ptr<World>& gameWorld,
	SessionServices& session,
	bool contentLoaded)
{
	const bool sessionOpen = gameWorld != nullptr;
	const bool wantSession = session.mode == SessionMode::Playing || session.mode == SessionMode::Paused;
	if (wantSession && !sessionOpen)
	{
		if (session.mode == SessionMode::Paused)
		{
			session.mode = SessionMode::Editor;
			return;
		}

		std::unique_ptr<World> started = std::make_unique<World>(editorWorld.Clone());
		const bool gameAdded = started->AddSubsystem(std::make_unique<GameSubsystem>());
		if (!gameAdded)
		{
			LOG_ERROR(LogCategory::Core, "Engine failed to register play world subsystems");
			session.mode = SessionMode::Editor;
			return;
		}

		if (auto begun = started->BeginPlay(contentLoaded, nullptr, nullptr); !begun)
		{
			LogResult(begun, LogCategory::Core);
			session.mode = SessionMode::Editor;
			return;
		}

		gameWorld = std::move(started);
		return;
	}

	if (!wantSession && sessionOpen)
	{
		gameWorld->EndPlay();
		gameWorld.reset();
	}
}
} // namespace

Engine::Engine(EngineConfig config)
	: m_config(std::move(config))
{
	if (!SpawnCamera(m_editorWorld, nullptr, nullptr).IsValid())
	{
		LOG_ERROR(LogCategory::Core, "Engine failed to spawn the editor camera");
	}
}

void Engine::RegisterSubsystems(SubsystemRegistry& registry) &&
{
	registry.Register(std::make_unique<Engine>(std::move(*this)));
	registry.Register(CreateEditorUiSubsystem());
}

const EngineConfig& Engine::GetConfig() const
{
	return m_config;
}

World& Engine::GetWorld()
{
	return m_gameWorld != nullptr ? *m_gameWorld : m_editorWorld;
}

const World& Engine::GetWorld() const
{
	return m_gameWorld != nullptr ? *m_gameWorld : m_editorWorld;
}

std::span<const char* const> Engine::GetDependencies() const
{
	return kDependencies;
}

Result<void> Engine::Initialize(SubsystemContext& /*ctx*/)
{
	if (SubsystemRegistry::FindCurrent<InputSubsystem>() == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"Engine requires InputSubsystem");
	}

	m_session.mode = SessionMode::Editor;
	m_worldServices.Publish(&m_editorWorld);
	return MakeOk();
}

void Engine::Tick(SubsystemContext& /*ctx*/, float deltaSeconds)
{
	InputServices* inputServices = nullptr;
	if (InputSubsystem* input = SubsystemRegistry::FindCurrent<InputSubsystem>())
	{
		inputServices = &input->GetInput();
	}
	ApplyPlaySession(m_editorWorld, m_gameWorld, m_session, m_content.loaded);

	World& world = GetWorld();
	const bool relativeMouse = world.Tick(
		deltaSeconds,
		m_session,
		m_worldServices,
		inputServices != nullptr ? inputServices->state : nullptr,
		inputServices != nullptr && inputServices->mouseCaptured,
		inputServices != nullptr && inputServices->keyboardCaptured);
	if (inputServices != nullptr)
	{
		inputServices->relativeMouse = relativeMouse;
	}
}

void Engine::FixedTick(SubsystemContext& /*ctx*/, float fixedDeltaSeconds)
{
	GetWorld().FixedTick(m_session, fixedDeltaSeconds);
}

void Engine::Shutdown(SubsystemContext& /*ctx*/)
{
	if (m_gameWorld != nullptr)
	{
		m_gameWorld->EndPlay();
		m_gameWorld.reset();
	}

	m_content = {};
	m_session = {};
	m_worldServices = {};
	m_debugStats = {};
}
