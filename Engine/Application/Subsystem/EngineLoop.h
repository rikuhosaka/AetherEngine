#pragma once

#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Application/Subsystem/FrameClock.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Core/Log/Result.h"

class IEngineLoopPlatform;
class IEngineLoopRender;
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
	Result<void> ResetFrameSlotResources(FrameContext& frameContext);
	Result<void> LoadInitialContent();
	Result<void> RenderFrame();
	Result<void> ResolveHosts();

	EngineLoopConfig m_config{};
	SubsystemRegistry m_registry{};
	SubsystemContext m_context{};
	FrameClock m_clock{};
	IEngineLoopPlatform* m_platform = nullptr;
	IEngineLoopRender* m_render = nullptr;
	bool m_running = false;
	bool m_pendingResize = false;
	uint32_t m_pendingWidth = 0;
	uint32_t m_pendingHeight = 0;
};
