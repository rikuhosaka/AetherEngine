#include "Runtime/Engine/Engine/Subsystem/Engine.h"

#include "Runtime/Engine/Engine/Subsystem/EditorUiSubsystem.h"
#include "Runtime/Engine/Application/Subsystem/SubsystemContext.h"
#include "Runtime/Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Runtime/Engine/Engine/Subsystem/InputSubsystem.h"
#include "Runtime/Engine/World/Component/CameraSystemComponent.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Engine/World/Subsystem/GameSubsystem.h"

#include <memory>
#include <utility>

namespace
{
constexpr const char* kDependencies[] = { "Input" };

void RejectPlayRequest(SessionServices& session)
{
	session.requestMode = SessionMode::Editor;
	session.mode = SessionMode::Editor;
}

// UI が書いた requestMode を、編集用 World とゲーム用 World の切り替えへ反映する。
// ボタンは依頼を書くだけで BeginPlay / EndPlay は呼ばない。反映はこの関数だけが行う。
// セッションを開くのは Playing への遷移だけ。最初から Paused を書かれてもセッションは開かず Editor に戻す。
// 開くときは編集用 World の文書をコピーし、Game を足してから BeginPlay する。
// カメラアクターは文書に入っているのでコピーが視点になる。無ければ BeginPlay が作る。
// 開いているセッションの一時停止は World を破棄しない。閉じるのは Editor へ戻ったとき。
// 反映できた結果は mode に書く。公開先のポインタは続く Tick が書き換える。
void ApplyPlaySession(
	World& editorWorld,
	std::unique_ptr<World>& gameWorld,
	SessionServices& session,
	bool contentLoaded)
{
	const bool sessionOpen = gameWorld != nullptr;
	const SessionMode requested = session.requestMode;
	const bool wantSession = requested == SessionMode::Playing || requested == SessionMode::Paused;
	if (wantSession && !sessionOpen)
	{
		if (requested == SessionMode::Paused)
		{
			RejectPlayRequest(session);
			return;
		}

		std::unique_ptr<World> started = std::make_unique<World>(editorWorld.Clone());
		const bool gameAdded = started->AddSubsystem(std::make_unique<GameSubsystem>());
		if (!gameAdded)
		{
			LOG_ERROR(LogCategory::Core, "Engine failed to register play world subsystems");
			RejectPlayRequest(session);
			return;
		}

		if (auto begun = started->BeginPlay(contentLoaded, nullptr, nullptr); !begun)
		{
			LogResult(begun, LogCategory::Core);
			RejectPlayRequest(session);
			return;
		}

		gameWorld = std::move(started);
		session.mode = SessionMode::Playing;
		return;
	}

	if (!wantSession && sessionOpen)
	{
		gameWorld->EndPlay();
		gameWorld.reset();
		session.mode = SessionMode::Editor;
		return;
	}

	session.mode = sessionOpen ? requested : SessionMode::Editor;
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

	m_session.requestMode = SessionMode::Editor;
	m_session.mode = SessionMode::Editor;
	m_worldServices.Publish(&m_editorWorld);
	return MakeOk();
}

void Engine::Tick(SubsystemContext& /*ctx*/, float deltaSeconds)
{
	ApplyPlaySession(m_editorWorld, m_gameWorld, m_session, m_content.loaded);
	m_worldServices.Publish(&GetWorld());
	GetWorld().Tick(deltaSeconds);
}

void Engine::FixedTick(SubsystemContext& /*ctx*/, float fixedDeltaSeconds)
{
	// 積分は Playing の間だけ。一時停止と編集中は World へ渡さない。
	if (m_session.mode != SessionMode::Playing)
	{
		return;
	}

	GetWorld().FixedTick(fixedDeltaSeconds);
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
