#include "Runtime/Launch/EngineLoop.h"

#include "Engine/Application/Services/DebugStatsServices.h"
#include "Engine/Application/Services/EditorCameraServices.h"
#include "Engine/Application/Services/SceneContentServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/UiServices.h"
#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Application/Subsystem/IEngineLoopPlatform.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Frame/FrameContext.h"
#include "Engine/Platform/WindowSubsystem.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/World/World.h"
#include "Engine/World/WorldExtract.h"

#include <cstring>
#include <vector>

namespace
{
struct PublishedWorld
{
	World* world = nullptr;
	WorldServices* services = nullptr;
	SceneContentServices* content = nullptr;
	SessionServices* session = nullptr;
	DebugStatsServices* stats = nullptr;
};

[[nodiscard]] PublishedWorld FindPublishedWorld(SubsystemContext& context)
{
	PublishedWorld published{};
	published.services = context.GetService<WorldServices>();
	if (published.services == nullptr || published.services->world == nullptr)
	{
		return {};
	}

	published.world = published.services->world;
	published.content = context.GetService<SceneContentServices>();
	published.session = context.GetService<SessionServices>();
	published.stats = context.GetService<DebugStatsServices>();
	return published;
}
} // namespace

EngineLoop::EngineLoop(EngineLoopConfig config, SubsystemRegistry registry)
	: m_config(std::move(config))
	, m_registry(std::move(registry))
{
}

Result<void> EngineLoop::Initialize()
{
	m_context.RegisterService(&m_registry);

	if (auto built = m_registry.BuildOrders(); !built)
	{
		return built;
	}

	for (ISubsystem* subsystem : m_registry.GetInitOrder())
	{
		if (subsystem == nullptr)
		{
			continue;
		}

		if (auto initResult = subsystem->Initialize(m_context); !initResult)
		{
			LOG_ERROR(LogCategory::Core, std::string("Subsystem '") + std::string(subsystem->GetName()) + "' failed to initialize");
			LogResult(initResult, LogCategory::Core);
			ShutdownSession();
			return initResult;
		}

		if (std::strcmp(subsystem->GetName(), "Window") == 0)
		{
			const WindowServices& windowServices = static_cast<WindowSubsystem*>(subsystem)->GetServices();
			if (auto renderResult = m_render.Initialize(
					windowServices.hwnd,
					windowServices.clientWidth,
					windowServices.clientHeight,
					m_config,
					m_context);
				!renderResult)
			{
				LogResult(renderResult, LogCategory::Core);
				ShutdownSession();
				return renderResult;
			}
		}
	}

	if (auto postInitResult = m_registry.PostInitializeAll(m_context); !postInitResult)
	{
		ShutdownSession();
		return postInitResult;
	}

	if (auto resolveResult = ResolveHosts(); !resolveResult)
	{
		ShutdownSession();
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
	}

	if (m_platform == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"EngineLoop requires a platform subsystem");
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
		TickFixedSubsystems();

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

	const FixedStepAdvance advance = AdvanceFixedSteps(
		m_simulationAccumulator,
		deltaSeconds,
		EngineConstants::kFixedDeltaSeconds,
		EngineConstants::kMaxFixedStepsPerFrame);
	m_fixedStepCount = advance.stepCount;
	m_context.SetInterpolationAlpha(advance.alpha);
}

Result<void> EngineLoop::ApplyPendingResize()
{
	auto* windowServices = m_context.GetService<WindowServices>();
	if (windowServices == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"EngineLoop::ApplyPendingResize missing required services");
	}

	if (m_pendingWidth == 0 || m_pendingHeight == 0)
	{
		m_pendingResize = false;
		return MakeOk();
	}

	if (auto resizeResult = m_render.Resize(m_pendingWidth, m_pendingHeight); !resizeResult)
	{
		LogResult(resizeResult, LogCategory::Core);
		return resizeResult;
	}

	for (ISubsystem* subsystem : m_registry.GetInitOrder())
	{
		if (auto subsystemResize = subsystem->OnResize(m_context, m_pendingWidth, m_pendingHeight); !subsystemResize)
		{
			LogResult(subsystemResize, LogCategory::Core);
			return subsystemResize;
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

void EngineLoop::TickFixedSubsystems()
{
	for (uint32_t step = 0; step < m_fixedStepCount; ++step)
	{
		for (ISubsystem* subsystem : m_registry.GetTickOrder())
		{
			if (subsystem != nullptr)
			{
				subsystem->FixedTick(m_context, EngineConstants::kFixedDeltaSeconds);
			}
		}
	}
}

Result<void> EngineLoop::LoadInitialContent()
{
	const PublishedWorld published = FindPublishedWorld(m_context);
	if (published.world == nullptr)
	{
		return MakeOk();
	}

	if (published.content == nullptr || published.services == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"EngineLoop::LoadInitialContent missing world services");
	}

	auto recording = m_render.BeginRecording();
	if (!recording)
	{
		return MakeFail(recording.error.code, recording.error.message);
	}

	FrameContext* frameContext = recording.value;
	m_context.SetFrameSlot(frameContext->frameIndex);

	if (auto loadResult = published.world->LoadContent(
			*published.content, *published.services, m_context, *frameContext, frameContext->graphicsCommandList);
		!loadResult)
	{
		m_render.AbortRecording();
		return loadResult;
	}

	return m_render.SubmitAndWait();
}

Result<void> EngineLoop::RenderFrame()
{
	auto* windowServices = m_context.GetService<WindowServices>();
	if (windowServices != nullptr
		&& (windowServices->isMinimized || windowServices->clientWidth == 0 || windowServices->clientHeight == 0))
	{
		return MakeOk();
	}

	const uint32_t width = windowServices != nullptr ? windowServices->clientWidth : 0;
	const uint32_t height = windowServices != nullptr ? windowServices->clientHeight : 0;
	m_context.SetFrameSlot(m_render.GetRHI().currentFrameSlot);

	std::vector<ExtractedObject> extractedObjects;
	ExtractedView extractedView{};
	ExtractedLighting extractedLighting{};
	const PublishedWorld published = FindPublishedWorld(m_context);
	if (published.world != nullptr)
	{
		if (published.session == nullptr || published.stats == nullptr)
		{
			return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
				"EngineLoop::RenderFrame missing world services");
		}

		ExtractView(
			*published.world, extractedView, width, height, m_context.GetService<EditorCameraServices>());
		ExtractLighting(*published.world, extractedLighting);
		Extract(
			*published.world,
			extractedObjects,
			m_context.GetInterpolationAlpha(),
			*published.session,
			*published.stats);
	}

	RHIImGui* imgui = nullptr;
	if (auto* uiServices = m_context.GetService<UiServices>())
	{
		imgui = uiServices->imgui;
	}

	if (auto renderResult = m_render.Render(
			width,
			height,
			extractedView,
			extractedLighting,
			extractedObjects,
			imgui);
		!renderResult)
	{
		return renderResult;
	}

	m_context.SetFrameSlot(m_render.GetRHI().currentFrameSlot);
	return MakeOk();
}

void EngineLoop::ShutdownSession()
{
	const std::span<ISubsystem* const> order = m_registry.GetInitOrder();
	for (size_t index = order.size(); index > 0; --index)
	{
		ISubsystem* subsystem = order[index - 1];
		if (subsystem == nullptr || std::strcmp(subsystem->GetName(), "Window") == 0)
		{
			continue;
		}

		subsystem->Shutdown(m_context);
	}

	m_render.Shutdown(m_context);

	for (ISubsystem* subsystem : order)
	{
		if (subsystem != nullptr && std::strcmp(subsystem->GetName(), "Window") == 0)
		{
			subsystem->Shutdown(m_context);
			break;
		}
	}

	m_context.UnregisterService(&m_registry);
	m_platform = nullptr;
}

void EngineLoop::Shutdown()
{
	if (!m_running)
	{
		return;
	}

	m_running = false;
	ShutdownSession();
}
