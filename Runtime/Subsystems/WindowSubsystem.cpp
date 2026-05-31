#include "Runtime/Subsystems/WindowSubsystem.h"

#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Platform/GameWindow.h"

WindowSubsystem::WindowSubsystem(EngineLoopConfig config)
	: m_config(std::move(config))
{
}

Result<void> WindowSubsystem::Initialize(SubsystemContext& ctx)
{
	GameWindow::CreateDesc desc{};
	desc.hInstance = m_config.hInstance;
	desc.clientWidth = m_config.initialWidth;
	desc.clientHeight = m_config.initialHeight;

	auto windowResult = GameWindow::Create(desc);
	if (!windowResult)
	{
		return MakeFail(windowResult.error.code, windowResult.error.message);
	}

	m_window = std::move(windowResult.value);
	SyncServicesFromWindow();
	ctx.RegisterService(&m_services);
	return MakeOk();
}

void WindowSubsystem::Shutdown(SubsystemContext& /*ctx*/)
{
	m_window.reset();
	m_services = {};
}

bool WindowSubsystem::ProcessMessages()
{
	if (m_window == nullptr)
	{
		return false;
	}

	const bool running = m_window->ProcessMessages();
	SyncServicesFromWindow();
	return running;
}

bool WindowSubsystem::HasPendingResize() const
{
	return m_window != nullptr && m_window->HasPendingResize();
}

void WindowSubsystem::ClearPendingResize()
{
	if (m_window != nullptr)
	{
		m_window->ClearPendingResize();
	}
}

uint32_t WindowSubsystem::GetPendingWidth() const
{
	return m_window != nullptr ? m_window->GetPendingWidth() : 0;
}

uint32_t WindowSubsystem::GetPendingHeight() const
{
	return m_window != nullptr ? m_window->GetPendingHeight() : 0;
}

void WindowSubsystem::SyncServicesFromWindow()
{
	if (m_window == nullptr)
	{
		return;
	}

	m_services.hwnd = m_window->GetHwnd();
	m_services.hInstance = m_config.hInstance;
	m_services.clientWidth = m_window->GetClientWidth();
	m_services.clientHeight = m_window->GetClientHeight();
	m_services.isMinimized = m_window->IsMinimized();
}
