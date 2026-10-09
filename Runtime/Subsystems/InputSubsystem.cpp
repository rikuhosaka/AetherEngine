#include "Runtime/Subsystems/InputSubsystem.h"

#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Platform/Win32InputDevice.h"

namespace
{
constexpr const char* kDependencies[] = { "Window" };
} // namespace

InputSubsystem::InputSubsystem() = default;

InputSubsystem::~InputSubsystem() = default;

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

	if (m_device == nullptr)
	{
		m_device = std::make_unique<Win32InputDevice>();
	}

	m_device->Reset(windowServices->hwnd, windowServices->clientWidth, windowServices->clientHeight, m_state);
	m_services.state = &m_state;
	ctx.RegisterService(&m_services);
	return MakeOk();
}

Result<void> InputSubsystem::OnResize(SubsystemContext& ctx, uint32_t width, uint32_t height)
{
	auto* windowServices = ctx.GetService<WindowServices>();
	if (windowServices == nullptr || windowServices->hwnd == nullptr || m_device == nullptr || width == 0 || height == 0)
	{
		return MakeOk();
	}

	m_device->Reset(windowServices->hwnd, width, height, m_state);
	m_device->RefreshRelativeMouse(windowServices->hwnd, width, height, m_state);
	return MakeOk();
}

void InputSubsystem::Tick(SubsystemContext& ctx, float /*deltaSeconds*/)
{
	auto* windowServices = ctx.GetService<WindowServices>();
	if (windowServices == nullptr || windowServices->hwnd == nullptr || m_device == nullptr)
	{
		return;
	}

	m_device->Update(
		windowServices->hwnd,
		windowServices->clientWidth,
		windowServices->clientHeight,
		m_state,
		m_services.relativeMouse);
}

void InputSubsystem::Shutdown(SubsystemContext& /*ctx*/)
{
	m_services = {};
	m_device.reset();
}
