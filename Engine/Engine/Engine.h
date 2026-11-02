#pragma once

#include "Engine/Application/Services/DebugStatsServices.h"
#include "Engine/Application/Services/SceneContentServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/Engine/EngineConfig.h"
#include "Engine/World/World.h"

class SubsystemRegistry;

// 責任: エディタが使う1つの World と、セッションおよび公開用サービスを持つ。
// 役割: UI が書いたモードをプレイの開始と終了へ反映し、可変 Tick と固定 Tick を World へ渡す。
// 所有: SubsystemRegistry が unique_ptr で持つ。World と各サービス構造体はメンバ。
//       サービスは SubsystemContext へ非所有ポインタで登録する。
class Engine final : public ISubsystem
{
public:
	explicit Engine(EngineConfig config);

	// 自分をレジストリへ移し、続けて編集用サブシステムを登録する。初期化前の rvalue から呼ぶ。
	// 呼び出し後、このインスタンスの中身はレジストリ側の Engine が所有する。
	void RegisterSubsystems(SubsystemRegistry& registry) &&;

	[[nodiscard]] const EngineConfig& GetConfig() const;
	[[nodiscard]] World& GetWorld();
	[[nodiscard]] const World& GetWorld() const;

	[[nodiscard]] const char* GetName() const override { return "Engine"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	void FixedTick(SubsystemContext& ctx, float fixedDeltaSeconds) override;
	void Shutdown(SubsystemContext& ctx) override;

private:
	// この Engine が所有する。Context へ登録するのは各サービスのアドレスだけで、World もここが寿命を持つ。
	EngineConfig m_config{};
	World m_world{};
	DebugStatsServices m_debugStats{};
	WorldServices m_worldServices{};
	SessionServices m_session{};
	SceneContentServices m_content{};
};
