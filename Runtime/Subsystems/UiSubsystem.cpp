#include "Runtime/Subsystems/UiSubsystem.h"

#include "Runtime/Subsystems/Outliner.h"
#include "Runtime/Subsystems/Details.h"

#include "Engine/Application/Services/DebugStatsServices.h"
#include "Engine/Application/Services/EditorSaveServices.h"
#include "Engine/Application/Services/InputServices.h"
#include "Engine/Application/Services/RHIServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemTypes.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHIImGui.h"
#include "Engine/World/World.h"

#include "imgui.h"

namespace
{
constexpr const char* kDependencies[] = { "Window", "RHI", "Display" };
} // namespace

UiSubsystem::UiSubsystem() = default;

UiSubsystem::~UiSubsystem() = default;

std::unique_ptr<ISubsystem> CreateUiSubsystem()
{
	return std::make_unique<UiSubsystem>();
}

std::span<const char* const> UiSubsystem::GetDependencies() const
{
	return kDependencies;
}

Result<void> UiSubsystem::Initialize(SubsystemContext& ctx)
{
	auto* windowServices = ctx.GetService<WindowServices>();
	auto* rhiServices = ctx.GetService<RHIServices>();
	if (windowServices == nullptr || windowServices->hwnd == nullptr || rhiServices == nullptr
		|| rhiServices->device == nullptr || rhiServices->graphicsQueue == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"UiSubsystem requires WindowServices and RHIServices");
	}

	RHIImGuiDesc desc{};
	desc.hwnd = windowServices->hwnd;
	desc.commandQueue = rhiServices->graphicsQueue;
	desc.framesInFlight = EngineConstants::kFrameInFlightCount;

	auto imguiResult = rhiServices->device->CreateImGui(desc);
	if (!imguiResult)
	{
		return MakeFail(imguiResult.error.code, imguiResult.error.message);
	}

	m_imgui = std::move(imguiResult.value);
	m_services.imgui = m_imgui.get();
	ctx.RegisterService(&m_services);
	ctx.RegisterService(&m_selection);
	return MakeOk();
}

void UiSubsystem::Tick(SubsystemContext& ctx, float /*deltaSeconds*/)
{
	if (m_imgui == nullptr)
	{
		return;
	}

	m_imgui->NewFrame();

	const float deltaSeconds = ctx.GetDeltaSeconds();
	const float framesPerSecond = deltaSeconds > 0.0f ? 1.0f / deltaSeconds : 0.0f;
	const auto* windowServices = ctx.GetService<WindowServices>();
	const uint32_t clientWidth = windowServices != nullptr ? windowServices->clientWidth : 0;
	const uint32_t clientHeight = windowServices != nullptr ? windowServices->clientHeight : 0;
	const auto* debugStats = ctx.GetService<DebugStatsServices>();
	const uint32_t extractedObjectCount = debugStats != nullptr ? debugStats->extractedObjectCount : 0;
	const auto* worldServices = ctx.GetService<WorldServices>();
	const World* world = worldServices != nullptr ? worldServices->world : nullptr;
	uint32_t entityCount = 0;
	if (world != nullptr)
	{
		world->ForEachAlive([&entityCount](EntityId, const Transform&, const Renderable&) {
			++entityCount;
		});
	}

	ImGui::Begin("Stats");
	ImGui::Text("FPS %.1f   %.2f ms", framesPerSecond, deltaSeconds * 1000.0f);
	ImGui::Text("Client %u x %u", clientWidth, clientHeight);
	ImGui::Text("Alpha %.3f", ctx.GetInterpolationAlpha());
	ImGui::Text("Frame slot %u", ctx.GetFrameSlot());
	ImGui::Text("Objects %u", extractedObjectCount);
	ImGui::Text("Entities %u", entityCount);
	if (auto* session = ctx.GetService<SessionServices>())
	{
		const char* modeName = "Editor";
		if (session->mode == SessionMode::Playing)
		{
			modeName = "Playing";
		}
		else if (session->mode == SessionMode::Paused)
		{
			modeName = "Paused";
		}
		ImGui::Text("Mode %s", modeName);
		if (ImGui::Button("Play"))
		{
			session->mode = SessionMode::Playing;
		}
		ImGui::SameLine();
		if (ImGui::Button("Pause") && session->mode == SessionMode::Playing)
		{
			session->mode = SessionMode::Paused;
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop"))
		{
			session->mode = SessionMode::Editor;
		}
		if (auto* editorSave = ctx.GetService<EditorSaveServices>())
		{
			ImGui::SameLine();
			ImGui::BeginDisabled(session->mode != SessionMode::Editor);
			if (ImGui::Button("Save"))
			{
				editorSave->requested = true;
			}
			ImGui::EndDisabled();
		}
	}
	ImGui::End();

	const auto* session = ctx.GetService<SessionServices>();
	const bool editable = session != nullptr && session->mode == SessionMode::Editor;
	DrawOutliner(world, &m_selection);
	DrawDetails(worldServices != nullptr ? worldServices->world : nullptr, &m_selection, editable);

	m_imgui->Render();

	if (InputServices* inputServices = ctx.GetService<InputServices>())
	{
		const auto* session = ctx.GetService<SessionServices>();
		if (session != nullptr && session->mode != SessionMode::Editor)
		{
			const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
			inputServices->mouseCaptured = hovered;
			inputServices->keyboardCaptured = hovered;
		}
		else
		{
			inputServices->mouseCaptured = m_imgui->WantCaptureMouse();
			inputServices->keyboardCaptured = m_imgui->WantCaptureKeyboard();
		}
	}
}

void UiSubsystem::Shutdown(SubsystemContext& ctx)
{
	ctx.UnregisterService(&m_selection);
	m_selection = {};
	ctx.UnregisterService(&m_services);
	m_services = {};
	m_imgui.reset();
}
