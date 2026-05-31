#include "Runtime/Subsystems/InputSubsystem.h"

#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Platform/InputManager.h"

namespace
{
constexpr const char* kDependencies[] = { "Window" };
} // namespace

std::unique_ptr<ISubsystem> CreateInputSubsystem()
{
	return std::make_unique<InputSubsystem>();
}

std::span<const char* const> InputSubsystem::GetDependencies() const
{
	return kDependencies;
}

Result<void> InputSubsystem::Initialize(SubsystemContext& ctx)
{
	auto* windowServices = ctx.GetService<WindowServices>();
	if (windowServices == nullptr || windowServices->hwnd == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"InputSubsystem requires WindowServices");
	}

	InputManager& input = InputManager::Get();
	input.Initialize(windowServices->hwnd, windowServices->clientWidth, windowServices->clientHeight);

	m_services.input = &input;
	ctx.RegisterService(&m_services);
	return MakeOk();
}

Result<void> InputSubsystem::OnResize(SubsystemContext& ctx, uint32_t width, uint32_t height)
{
	auto* windowServices = ctx.GetService<WindowServices>();
	if (windowServices == nullptr || windowServices->hwnd == nullptr || width == 0 || height == 0)
	{
		return MakeOk();
	}

	InputManager::Get().Initialize(windowServices->hwnd, width, height);
	return MakeOk();
}

void InputSubsystem::Tick(SubsystemContext& ctx, float /*deltaSeconds*/)
{
	auto* windowServices = ctx.GetService<WindowServices>();
	if (windowServices == nullptr || windowServices->hwnd == nullptr || m_services.input == nullptr)
	{
		return;
	}

	m_services.input->Update(
		windowServices->hwnd,
		windowServices->clientWidth,
		windowServices->clientHeight);
}

void InputSubsystem::Shutdown(SubsystemContext& /*ctx*/)
{
	m_services = {};
}
