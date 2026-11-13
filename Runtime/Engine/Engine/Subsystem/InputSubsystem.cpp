#include "Runtime/Engine/Engine/Subsystem/InputSubsystem.h"

#include "Runtime/Engine/Application/Subsystem/SubsystemContext.h"
#include "Runtime/Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Runtime/Engine/Engine/Subsystem/WindowSubsystem.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Core/Log/Result.h"
#include "Runtime/Core/Platform/Win32InputDevice.h"

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

Result<void> InputSubsystem::Initialize(SubsystemContext& /*ctx*/)
{
	WindowSubsystem* window = SubsystemRegistry::FindCurrent<WindowSubsystem>();
	const WindowServices* windowServices = window != nullptr ? &window->GetServices() : nullptr;
	if (windowServices == nullptr || windowServices->hwnd == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"InputSubsystem requires WindowSubsystem");
	}

	if (m_device == nullptr)
	{
		m_device = std::make_unique<Win32InputDevice>();
	}

	m_device->Reset(windowServices->hwnd, windowServices->clientWidth, windowServices->clientHeight, m_state);
	m_services.state = &m_state;
	return MakeOk();
}

Result<void> InputSubsystem::OnResize(SubsystemContext& /*ctx*/, uint32_t width, uint32_t height)
{
	WindowSubsystem* window = SubsystemRegistry::FindCurrent<WindowSubsystem>();
	const WindowServices* windowServices = window != nullptr ? &window->GetServices() : nullptr;
	if (windowServices == nullptr || windowServices->hwnd == nullptr || m_device == nullptr || width == 0 || height == 0)
	{
		return MakeOk();
	}

	m_device->Reset(windowServices->hwnd, width, height, m_state);
	m_device->RefreshRelativeMouse(windowServices->hwnd, width, height, m_state);
	return MakeOk();
}

void InputSubsystem::Tick(SubsystemContext& /*ctx*/, float /*deltaSeconds*/)
{
	WindowSubsystem* window = SubsystemRegistry::FindCurrent<WindowSubsystem>();
	const WindowServices* windowServices = window != nullptr ? &window->GetServices() : nullptr;
	if (windowServices == nullptr || windowServices->hwnd == nullptr || m_device == nullptr)
	{
		return;
	}

	const bool mouseCaptured = m_services.mouseCaptured;
	const bool keyboardCaptured = m_services.keyboardCaptured;
	m_device->Update(
		windowServices->hwnd,
		windowServices->clientWidth,
		windowServices->clientHeight,
		m_state,
		m_services.relativeMouse && !mouseCaptured,
		windowServices->mouseWheelDelta,
		windowServices->textInput,
		mouseCaptured,
		keyboardCaptured);
	m_services.mouseCaptured = false;
	m_services.keyboardCaptured = false;
}

void InputSubsystem::Shutdown(SubsystemContext& /*ctx*/)
{
	m_services = {};
	m_device.reset();
}
