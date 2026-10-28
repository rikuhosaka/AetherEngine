#include "Engine/Renderer/UiSubsystem.h"

#include "Engine/Application/Services/InputServices.h"
#include "Engine/Application/Services/RHIServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemTypes.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHIImGui.h"

#include "imgui.h"

namespace
{
constexpr const char* kDependencies[] = { "Window", "RHI", "Display" };
constexpr const char* kPresentDependencies[] = { "UI" };
} // namespace

UiSubsystem::UiSubsystem() = default;

UiSubsystem::~UiSubsystem() = default;

std::unique_ptr<ISubsystem> CreateUiSubsystem()
{
	return std::make_unique<UiSubsystem>();
}

std::unique_ptr<ISubsystem> CreateUiPresentSubsystem()
{
	return std::make_unique<UiPresentSubsystem>();
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
	return MakeOk();
}

void UiSubsystem::Tick(SubsystemContext& /*ctx*/, float /*deltaSeconds*/)
{
	if (m_imgui == nullptr)
	{
		return;
	}

	m_imgui->NewFrame();
}

void UiSubsystem::Shutdown(SubsystemContext& ctx)
{
	ctx.UnregisterService(&m_services);
	m_services = {};
	m_imgui.reset();
}

std::span<const char* const> UiPresentSubsystem::GetDependencies() const
{
	return kPresentDependencies;
}

Result<void> UiPresentSubsystem::Initialize(SubsystemContext& /*ctx*/)
{
	return MakeOk();
}

void UiPresentSubsystem::Tick(SubsystemContext& ctx, float /*deltaSeconds*/)
{
	auto* uiServices = ctx.GetService<UiServices>();
	if (uiServices == nullptr || uiServices->imgui == nullptr)
	{
		return;
	}

	uiServices->imgui->Render();

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
			inputServices->mouseCaptured = uiServices->imgui->WantCaptureMouse();
			inputServices->keyboardCaptured = uiServices->imgui->WantCaptureKeyboard();
		}
	}
}

void UiPresentSubsystem::Shutdown(SubsystemContext& /*ctx*/)
{
}
