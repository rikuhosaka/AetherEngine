#pragma once

#include "Engine/Application/Services/DebugStatsServices.h"
#include "Engine/Application/Services/SceneContentServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/Engine/EngineConfig.h"
#include "Engine/World/World.h"

class SubsystemRegistry;

class Engine final : public ISubsystem
{
public:
	explicit Engine(EngineConfig config);

	// Registers this engine, then the editor subsystems. Call on an rvalue before initialization.
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
	EngineConfig m_config{};
	World m_world{};
	DebugStatsServices m_debugStats{};
	WorldServices m_worldServices{};
	SessionServices m_session{};
	SceneContentServices m_content{};
};
