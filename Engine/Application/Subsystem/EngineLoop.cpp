#include "Engine/Application/Subsystem/EngineLoop.h"

#include "Engine/Application/Services/RHIServices.h"
#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/IEngineLoopPlatform.h"
#include "Engine/Application/Subsystem/IEngineLoopRender.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHIFence.h"

EngineLoop::EngineLoop(EngineLoopConfig config, SubsystemRegistry registry)
	: m_config(std::move(config))
	, m_registry(std::move(registry))
{
}

Result<void> EngineLoop::Initialize()
{
	m_context.RegisterService(&m_registry);

	if (auto initResult = m_registry.InitializeAll(m_context); !initResult)
	{
		return initResult;
	}

	if (auto postInitResult = m_registry.PostInitializeAll(m_context); !postInitResult)
	{
		m_registry.ShutdownAll(m_context);
		return postInitResult;
	}

	if (auto resolveResult = ResolveHosts(); !resolveResult)
	{
		m_registry.ShutdownAll(m_context);
		return resolveResult;
	}

	m_running = true;
	return MakeOk();
}

Result<void> EngineLoop::ResolveHosts()
{
	for (ISubsystem* subsystem : m_registry.GetInitOrder())
	{
		if (m_platform == nullptr)
		{
			m_platform = dynamic_cast<IEngineLoopPlatform*>(subsystem);
		}

		if (m_render == nullptr)
		{
			m_render = dynamic_cast<IEngineLoopRender*>(subsystem);
		}
	}

	if (m_platform == nullptr || m_render == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"EngineLoop requires platform and render subsystems");
	}

	return MakeOk();
}

int EngineLoop::Run()
{
	if (!m_running)
	{
		LOG_FATAL(LogCategory::Core, "EngineLoop::Run called before successful initialization");
		return -1;
	}

	m_clock.Reset();

	while (m_running)
	{
		if (!ProcessPlatformMessages())
		{
			break;
		}

		if (m_platform->HasPendingResize())
		{
			m_pendingResize = true;
			m_pendingWidth = m_platform->GetPendingWidth();
			m_pendingHeight = m_platform->GetPendingHeight();
			m_platform->ClearPendingResize();
		}

		UpdateClock();

		if (m_pendingResize)
		{
			if (auto resizeResult = ApplyPendingResize(); !resizeResult)
			{
				LogResult(resizeResult, LogCategory::Core);
				break;
			}
		}

		TickSubsystems();

		if (auto renderResult = RenderFrame(); !renderResult)
		{
			LogResult(renderResult, LogCategory::Renderer);
			break;
		}
	}

	Shutdown();
	return 0;
}

bool EngineLoop::ProcessPlatformMessages()
{
	return m_platform != nullptr && m_platform->ProcessPlatformMessages();
}

void EngineLoop::UpdateClock()
{
	const float deltaSeconds = m_clock.Tick(m_config.maxDeltaSeconds);
	m_context.SetDeltaSeconds(deltaSeconds);
}

Result<void> EngineLoop::ApplyPendingResize()
{
	auto* windowServices = m_context.GetService<WindowServices>();
	auto* rhiServices = m_context.GetService<RHIServices>();
	if (windowServices == nullptr || rhiServices == nullptr || rhiServices->frameFence == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"EngineLoop::ApplyPendingResize missing required services");
	}

	if (m_pendingWidth == 0 || m_pendingHeight == 0)
	{
		m_pendingResize = false;
		return MakeOk();
	}

	for (FrameContext& frameContext : rhiServices->frameContexts)
	{
		if (frameContext.fenceValue != 0)
		{
			rhiServices->frameFence->WaitCPU(frameContext.fenceValue);
		}
	}

	for (ISubsystem* subsystem : m_registry.GetInitOrder())
	{
		if (auto resizeResult = subsystem->OnResize(m_context, m_pendingWidth, m_pendingHeight); !resizeResult)
		{
			LogResult(resizeResult, LogCategory::Core);
			return resizeResult;
		}
	}

	windowServices->clientWidth = m_pendingWidth;
	windowServices->clientHeight = m_pendingHeight;

	m_pendingResize = false;
	return MakeOk();
}

void EngineLoop::TickSubsystems()
{
	const float deltaSeconds = m_context.GetDeltaSeconds();
	for (ISubsystem* subsystem : m_registry.GetTickOrder())
	{
		if (subsystem != nullptr)
		{
			subsystem->Tick(m_context, deltaSeconds);
		}
	}
}

Result<void> EngineLoop::RenderFrame()
{
	return m_render != nullptr ? m_render->RenderFrame(m_context) : MakeOk();
}

void EngineLoop::Shutdown()
{
	if (m_running)
	{
		m_registry.ShutdownAll(m_context);
		m_running = false;
	}

	m_platform = nullptr;
	m_render = nullptr;
}
