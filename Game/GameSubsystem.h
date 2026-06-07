#pragma once

#include "Engine/Application/Subsystem/ISceneExtractor.h"
#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"

#include <memory>

class IGameModule;
class GameHost;

class GameSubsystem final : public ISubsystem, public ISceneExtractor
{
public:
	GameSubsystem();
	~GameSubsystem() override;

	[[nodiscard]] const char* GetName() const override { return "Game"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] SubsystemTickGroup GetTickGroup() const override { return SubsystemTickGroup::Game; }
	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	Result<void> PostInitialize(SubsystemContext& ctx) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	Result<void> PrepareRender(
		SubsystemContext& ctx,
		FrameContext& frameContext,
		RHICommandList* commandList) override;
	void Extract(std::vector<ExtractedObject>& outObjects) override;
	void ExtractView(ExtractedView& outView) override;
	void ExtractLighting(ExtractedLighting& outLighting) override;
	void Shutdown(SubsystemContext& ctx) override;

private:
	std::unique_ptr<IGameModule> m_module{};
	std::unique_ptr<GameHost> m_host{};
};

std::unique_ptr<ISubsystem> CreateGameSubsystem();
