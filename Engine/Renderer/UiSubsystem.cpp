#include "Engine/Renderer/UiSubsystem.h"

#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Engine/Engine.h"
#include "Engine/Platform/InputSubsystem.h"

namespace
{
constexpr const char* kDependencies[] = { "Window" };
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

Result<void> UiSubsystem::Initialize(SubsystemContext& /*ctx*/)
{
	return MakeOk();
}

void UiSubsystem::Tick(SubsystemContext& /*ctx*/, float /*deltaSeconds*/)
{
}

void UiSubsystem::Shutdown(SubsystemContext& /*ctx*/)
{
	m_services = {};
}

std::span<const char* const> UiPresentSubsystem::GetDependencies() const
{
	return kPresentDependencies;
}

Result<void> UiPresentSubsystem::Initialize(SubsystemContext& /*ctx*/)
{
	return MakeOk();
}

void UiPresentSubsystem::Tick(SubsystemContext& /*ctx*/, float /*deltaSeconds*/)
{
	UiSubsystem* ui = SubsystemRegistry::FindCurrent<UiSubsystem>();
	const UiServices* uiServices = ui != nullptr ? &ui->GetUi() : nullptr;
	if (uiServices == nullptr || !uiServices->frameStarted)
	{
		return;
	}

	if (InputSubsystem* input = SubsystemRegistry::FindCurrent<InputSubsystem>())
	{
		InputServices& inputServices = input->GetInput();
		const SessionServices* session = nullptr;
		if (Engine* engine = SubsystemRegistry::FindCurrent<Engine>())
		{
			session = &engine->GetSession();
		}
		if (session != nullptr && session->mode != SessionMode::Editor)
		{
			inputServices.mouseCaptured = uiServices->anyWindowHovered;
			inputServices.keyboardCaptured = uiServices->anyWindowHovered;
		}
		else
		{
			inputServices.mouseCaptured = uiServices->wantCaptureMouse;
			inputServices.keyboardCaptured = uiServices->wantCaptureKeyboard;
		}
	}
}

void UiPresentSubsystem::Shutdown(SubsystemContext& /*ctx*/)
{
}
