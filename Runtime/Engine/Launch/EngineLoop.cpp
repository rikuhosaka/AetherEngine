#include "Runtime/Engine/Launch/EngineLoop.h"

#include "Runtime/Engine/Application/Subsystem/IEngineLoopPlatform.h"
#include "Runtime/Engine/EngineInstance/Subsystem/Engine.h"
#include "Runtime/Engine/EngineInstance/Subsystem/SceneDocumentSubsystem.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/EngineInstance/Subsystem/WindowSubsystem.h"
#include "Runtime/Engine/World/ExtractedLighting.h"
#include "Runtime/Engine/World/World.h"
#include "Runtime/Engine/World/WorldExtract.h"

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

[[nodiscard]] PublishedWorld FindPublishedWorld()
{
	PublishedWorld published{};
	Engine* engine = SubsystemRegistry::FindCurrent<Engine>();
	if (engine == nullptr)
	{
		return {};
	}

	published.services = &engine->GetWorldServices();
	if (published.services->world == nullptr)
	{
		return {};
	}

	published.world = published.services->world;
	published.content = &engine->GetSceneContent();
	published.session = &engine->GetSession();
	published.stats = &engine->GetDebugStats();
	return published;
}

void PublishUiCapture(const UiFrameCapture& capture)
{
	UiSubsystem* ui = SubsystemRegistry::FindCurrent<UiSubsystem>();
	if (ui == nullptr)
	{
		return;
	}

	UiServices& uiServices = ui->GetUi();
	uiServices.wantCaptureMouse = capture.wantCaptureMouse;
	uiServices.wantCaptureKeyboard = capture.wantCaptureKeyboard;
	uiServices.anyWindowHovered = capture.anyWindowHovered;
}
} // namespace

EngineLoop::EngineLoop(EngineLoopConfig config, SubsystemRegistry registry)
	: m_config(std::move(config))
	, m_registry(std::move(registry))
{
}

Result<void> EngineLoop::Initialize()
{
	SubsystemRegistry::SetCurrent(&m_registry);

	if (auto built = m_registry.BuildOrders(); !built)
	{
		SubsystemRegistry::SetCurrent(nullptr);
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
	WindowSubsystem* window = SubsystemRegistry::FindCurrent<WindowSubsystem>();
	if (window == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"EngineLoop::ApplyPendingResize missing WindowSubsystem");
	}

	WindowServices& windowServices = window->GetServices();

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

	windowServices.clientWidth = m_pendingWidth;
	windowServices.clientHeight = m_pendingHeight;

	m_pendingResize = false;
	return MakeOk();
}

void EngineLoop::TickSubsystems()
{
	const float deltaSeconds = m_context.GetDeltaSeconds();
	bool uiFrameOpen = false;
	for (ISubsystem* subsystem : m_registry.GetTickOrder())
	{
		if (subsystem == nullptr)
		{
			continue;
		}

		const char* name = subsystem->GetName();
		if (uiFrameOpen && std::strcmp(name, "UiPresent") == 0)
		{
			PublishUiCapture(m_render.EndUiFrame());
			uiFrameOpen = false;
		}

		if (std::strcmp(name, "UI") == 0)
		{
			uiFrameOpen = m_render.BeginUiFrame();
			if (UiSubsystem* ui = SubsystemRegistry::FindCurrent<UiSubsystem>())
			{
				ui->GetUi().frameStarted = uiFrameOpen;
			}
		}

		subsystem->Tick(m_context, deltaSeconds);
	}

	if (uiFrameOpen)
	{
		PublishUiCapture(m_render.EndUiFrame());
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
	const PublishedWorld published = FindPublishedWorld();
	if (published.world == nullptr)
	{
		return MakeOk();
	}

	if (published.content == nullptr || published.services == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"EngineLoop::LoadInitialContent missing the engine world");
	}

	auto recording = m_render.BeginRecording();
	if (!recording)
	{
		return MakeFail(recording.error.code, recording.error.message);
	}

	m_context.SetFrameSlot(recording.value);

	SceneDocumentSubsystem* scene = SubsystemRegistry::FindCurrent<SceneDocumentSubsystem>();
	std::vector<WorldDocumentBinding> spawned{};
	if (scene != nullptr)
	{
		if (auto read = scene->ReadScene(m_config.scenePath); !read)
		{
			m_render.AbortRecording();
			return read;
		}

		Result<std::vector<WorldDocumentBinding>> instantiated = m_render.UploadPendingScene(
			*scene,
			*published.world);
		if (!instantiated)
		{
			m_render.AbortRecording();
			scene->DiscardPending();
			return MakeFail(instantiated.error.code, instantiated.error.message);
		}

		spawned = std::move(instantiated.value);
	}

	if (auto submitted = m_render.SubmitAndWait(); !submitted)
	{
		for (const WorldDocumentBinding& binding : spawned)
		{
			published.world->Destroy(binding.entity);
		}

		if (scene != nullptr)
		{
			scene->DiscardPending();
		}

		return submitted;
	}

	if (scene != nullptr && scene->HasPending())
	{
		scene->CommitPending();
		published.content->loaded = true;
	}

	return MakeOk();
}

Result<void> EngineLoop::RenderFrame()
{
	WindowSubsystem* window = SubsystemRegistry::FindCurrent<WindowSubsystem>();
	const WindowServices* windowServices = window != nullptr ? &window->GetServices() : nullptr;
	if (windowServices != nullptr
		&& (windowServices->isMinimized || windowServices->clientWidth == 0 || windowServices->clientHeight == 0))
	{
		return MakeOk();
	}

	const uint32_t width = windowServices != nullptr ? windowServices->clientWidth : 0;
	const uint32_t height = windowServices != nullptr ? windowServices->clientHeight : 0;
	m_context.SetFrameSlot(m_render.GetFrameSlot());

	std::vector<WorldExtractedObject> extractedObjects;
	ExtractedView extractedView{};
	ExtractedLighting extractedLighting{};
	const PublishedWorld published = FindPublishedWorld();
	if (published.world != nullptr)
	{
		if (published.session == nullptr || published.stats == nullptr)
		{
			return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
				"EngineLoop::RenderFrame missing the engine world");
		}

		ExtractView(*published.world, extractedView, width, height);
		ExtractLighting(*published.world, extractedLighting);
		// 一時停止中に補間すると、停止した瞬間の中間姿勢が残る。最新ステップをそのまま出す。
		const float alpha = published.session->mode == SessionMode::Paused
			? 1.0f
			: m_context.GetInterpolationAlpha();
		Extract(
			*published.world,
			extractedObjects,
			alpha,
			*published.stats);
	}

	if (auto renderResult = m_render.Render(
			width,
			height,
			extractedView,
			extractedLighting,
			extractedObjects);
		!renderResult)
	{
		return renderResult;
	}

	m_context.SetFrameSlot(m_render.GetFrameSlot());
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

	SubsystemRegistry::SetCurrent(nullptr);
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
