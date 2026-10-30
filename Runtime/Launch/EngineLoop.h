#pragma once

#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Application/Subsystem/FrameClock.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/RenderSystem.h"

class IEngineLoopPlatform;
struct FrameContext;

class EngineLoop
{
public:
	EngineLoop(EngineLoopConfig config, SubsystemRegistry registry);

	Result<void> Initialize();
	int Run();
	void Shutdown();

private:
	bool ProcessPlatformMessages();
	void UpdateClock();
	Result<void> ApplyPendingResize();
	void TickSubsystems();
	void TickFixedSubsystems();
	Result<void> LoadInitialContent();
	Result<void> RenderFrame();
	Result<void> ResolveHosts();
	void ShutdownSession();

	EngineLoopConfig m_config{};
	SubsystemRegistry m_registry{};
	SubsystemContext m_context{};
	RenderSystem m_render{};
	FrameClock m_clock{};
	double m_simulationAccumulator = 0.0;
	uint32_t m_fixedStepCount = 0;
	IEngineLoopPlatform* m_platform = nullptr;
	bool m_running = false;
	bool m_pendingResize = false;
	uint32_t m_pendingWidth = 0;
	uint32_t m_pendingHeight = 0;
};
