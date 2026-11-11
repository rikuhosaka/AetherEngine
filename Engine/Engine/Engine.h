#pragma once

#include "Engine/World/DebugStatsServices.h"
#include "Engine/World/SceneContentServices.h"
#include "Engine/World/SessionServices.h"
#include "Engine/World/WorldServices.h"
#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/Engine/EngineConfig.h"
#include "Engine/World/World.h"

#include <memory>

class SubsystemRegistry;

// 責任: 起動時からある編集用 World と、プレイ中だけのゲーム用 World、セッションと公開用サービスを持つ。
// 役割: UI が書いた依頼をプレイの開始と終了へ反映し、可変 Tick と固定 Tick を今の World へ渡す。
// 所有: SubsystemRegistry が unique_ptr で持つ。編集用 World は常にメンバ。ゲーム用 World はプレイ中だけ unique_ptr で持つ。
//       公開されるのは今 Tick している World。他のサブシステムはレジストリからこの Engine を引いて読む。
class Engine final : public ISubsystem
{
public:
	explicit Engine(EngineConfig config);

	// 自分をレジストリへ移し、続けて編集用サブシステムを登録する。初期化前の rvalue から呼ぶ。
	// 呼び出し後、このインスタンスの中身はレジストリ側の Engine が所有する。
	void RegisterSubsystems(SubsystemRegistry& registry) &&;

	[[nodiscard]] const EngineConfig& GetConfig() const;
	// プレイ中と一時停止中はゲーム用 World。それ以外は編集用 World。
	[[nodiscard]] World& GetWorld();
	[[nodiscard]] const World& GetWorld() const;
	[[nodiscard]] WorldServices& GetWorldServices() noexcept { return m_worldServices; }
	[[nodiscard]] SessionServices& GetSession() noexcept { return m_session; }
	[[nodiscard]] SceneContentServices& GetSceneContent() noexcept { return m_content; }
	[[nodiscard]] DebugStatsServices& GetDebugStats() noexcept { return m_debugStats; }

	[[nodiscard]] const char* GetName() const override { return "Engine"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	void FixedTick(SubsystemContext& ctx, float fixedDeltaSeconds) override;
	void Shutdown(SubsystemContext& ctx) override;

private:
	// この Engine が所有する。
	// 編集用 World は起動時に作り、カメラアクターを1つ置く。ゲーム用 World はプレイ開始時に編集用の文書をコピーして作り、
	// Game を足す。写したカメラが無ければ BeginPlay が作る。停止するとゲーム用 World は破棄する。
	EngineConfig m_config{};
	World m_editorWorld{};
	std::unique_ptr<World> m_gameWorld{};
	DebugStatsServices m_debugStats{};
	WorldServices m_worldServices{};
	SessionServices m_session{};
	SceneContentServices m_content{};
};
