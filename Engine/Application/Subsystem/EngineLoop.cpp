#include "Engine/Application/Subsystem/EngineLoop.h"

#include "Engine/Application/Services/DisplayServices.h"
#include "Engine/Application/Services/RHIServices.h"
#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/IEngineLoopPlatform.h"
#include "Engine/Application/Subsystem/IEngineLoopRender.h"
#include "Engine/Application/Subsystem/ISceneExtractor.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Graphics/DisplayContext.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/RHI/Interface/RHIBarrierDebug.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Interface/RHICommandQueue.h"
#include "Engine/RHI/Interface/RHIFence.h"
#include "Engine/RHI/Interface/RHITransientDescriptorAllocator.h"
#include "Engine/RHI/Interface/RHIUploadBuffer.h"

#include <vector>

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

	if (auto loadResult = LoadInitialContent(); !loadResult)
	{
		LogResult(loadResult, LogCategory::Core);
		Shutdown();
		return -1;
	}

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

Result<void> EngineLoop::ResetFrameSlotResources(FrameContext& frameContext)
{
	RHICommandList* commandList = frameContext.graphicsCommandList;
	if (commandList == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"EngineLoop requires a command list");
	}

	commandList->Reset();
	if (frameContext.uploadBuffer != nullptr)
	{
		frameContext.uploadBuffer->Reset();
	}
	if (frameContext.transientDescriptors != nullptr)
	{
		frameContext.transientDescriptors->Reset();
	}

	return MakeOk();
}

Result<void> EngineLoop::LoadInitialContent()
{
	auto* rhiServices = m_context.GetService<RHIServices>();
	if (rhiServices == nullptr || rhiServices->frameFence == nullptr || rhiServices->graphicsQueue == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"EngineLoop::LoadInitialContent missing required services");
	}

	ISceneExtractor* sceneExtractor = m_registry.GetSceneExtractor();
	if (sceneExtractor == nullptr)
	{
		return MakeOk();
	}

	const uint32_t slot = rhiServices->currentFrameSlot;
	FrameContext& frameContext = rhiServices->frameContexts[slot];
	frameContext.frameIndex = slot;
	m_context.SetFrameSlot(slot);

	if (rhiServices->barrierDebug != nullptr)
	{
		rhiServices->barrierDebug->BeginFrame();
	}

	if (auto resetResult = ResetFrameSlotResources(frameContext); !resetResult)
	{
		if (rhiServices->barrierDebug != nullptr)
		{
			rhiServices->barrierDebug->EndFrame();
		}
		return resetResult;
	}

	RHICommandList* commandList = frameContext.graphicsCommandList;
	if (auto loadResult = sceneExtractor->LoadContent(m_context, frameContext, commandList); !loadResult)
	{
		commandList->Close();
		if (rhiServices->barrierDebug != nullptr)
		{
			rhiServices->barrierDebug->EndFrame();
		}
		return loadResult;
	}

	commandList->Close();
	rhiServices->graphicsQueue->ExecuteCommandLists({ commandList });
	const uint64_t fenceValue = rhiServices->graphicsQueue->Signal(rhiServices->frameFence);
	rhiServices->frameFence->WaitCPU(fenceValue);
	frameContext.fenceValue = 0;

	if (rhiServices->barrierDebug != nullptr)
	{
		rhiServices->barrierDebug->EndFrame();
	}

	return MakeOk();
}

Result<void> EngineLoop::RenderFrame()
{
	auto* rhiServices = m_context.GetService<RHIServices>();
	auto* displayServices = m_context.GetService<DisplayServices>();
	auto* windowServices = m_context.GetService<WindowServices>();
	if (m_render == nullptr || rhiServices == nullptr || rhiServices->frameFence == nullptr
		|| rhiServices->graphicsQueue == nullptr || displayServices == nullptr
		|| displayServices->display == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"EngineLoop::RenderFrame missing required services");
	}

	if (windowServices != nullptr
		&& (windowServices->isMinimized || windowServices->clientWidth == 0 || windowServices->clientHeight == 0))
	{
		return MakeOk();
	}

	const uint32_t slot = rhiServices->currentFrameSlot;
	FrameContext& frameContext = rhiServices->frameContexts[slot];
	frameContext.frameIndex = slot;

	if (frameContext.fenceValue != 0)
	{
		rhiServices->frameFence->WaitCPU(frameContext.fenceValue);
	}

	m_context.SetFrameSlot(slot);

	if (rhiServices->barrierDebug != nullptr)
	{
		rhiServices->barrierDebug->BeginFrame();
	}

	displayServices->display->BeginFrame(frameContext);

	if (auto resetResult = ResetFrameSlotResources(frameContext); !resetResult)
	{
		if (rhiServices->barrierDebug != nullptr)
		{
			rhiServices->barrierDebug->EndFrame();
		}
		return resetResult;
	}

	RHICommandList* commandList = frameContext.graphicsCommandList;

	std::vector<ExtractedObject> extractedObjects;
	ExtractedView extractedView{};
	ExtractedLighting extractedLighting{};
	if (ISceneExtractor* sceneExtractor = m_registry.GetSceneExtractor())
	{
		if (auto prepareResult = sceneExtractor->PrepareRender(m_context, frameContext, commandList);
			!prepareResult)
		{
			return prepareResult;
		}

		sceneExtractor->ExtractView(extractedView);
		sceneExtractor->ExtractLighting(extractedLighting);
		sceneExtractor->Extract(extractedObjects);
	}

	if (auto drawResult = m_render->DrawFrame(
			m_context,
			frameContext,
			commandList,
			extractedView,
			extractedLighting,
			extractedObjects);
		!drawResult)
	{
		return drawResult;
	}

	commandList->Close();
	rhiServices->graphicsQueue->ExecuteCommandLists({ commandList });
	frameContext.fenceValue = rhiServices->graphicsQueue->Signal(rhiServices->frameFence);

	displayServices->display->Present(m_config.vsync ? 1u : 0u, 0u);
	if (rhiServices->barrierDebug != nullptr)
	{
		rhiServices->barrierDebug->EndFrame();
	}

	rhiServices->currentFrameSlot = (slot + 1) % RHIServices::kFrameCount;
	m_context.SetFrameSlot(rhiServices->currentFrameSlot);
	return MakeOk();
}

void EngineLoop::Shutdown()
{
	if (m_running)
	{
		m_registry.ShutdownAll(m_context);
		m_running = false;
	}

	m_context.UnregisterService(&m_registry);
	m_platform = nullptr;
	m_render = nullptr;
}
