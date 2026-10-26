#pragma once

#include "Engine/Application/Services/DebugStatsServices.h"
#include "Engine/Application/Services/SceneContentServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Application/Subsystem/ISceneExtractor.h"
#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/World/GameScene.h"
#include "Engine/World/World.h"

#include <memory>
#include <optional>

class WorldSubsystem final : public ISubsystem, public ISceneExtractor
{
public:
	[[nodiscard]] const char* GetName() const override { return "World"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	Result<void> PostInitialize(SubsystemContext& ctx) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	void FixedTick(SubsystemContext& ctx, float fixedDeltaSeconds) override;
	Result<void> LoadContent(
		SubsystemContext& ctx,
		FrameContext& frameContext,
		RHICommandList* commandList) override;
	void Extract(std::vector<ExtractedObject>& outObjects) override;
	void ExtractView(ExtractedView& outView) override;
	void ExtractLighting(ExtractedLighting& outLighting) override;
	void Shutdown(SubsystemContext& ctx) override;

private:
	void ApplySession();
	void PublishWorld();
	[[nodiscard]] Result<void> BeginPlay();
	void EndPlay();
	[[nodiscard]] Result<void> Step(float fixedDeltaSeconds);

	SubsystemContext* m_context = nullptr;
	World m_editor{};
	std::optional<GameScene> m_game{};
	DebugStatsServices m_debugStats{};
	WorldServices m_worldServices{};
	SessionServices m_session{};
	SessionMode m_appliedMode = SessionMode::Editor;
	SceneContentServices m_content{};
};

std::unique_ptr<ISubsystem> CreateWorldSubsystem();
