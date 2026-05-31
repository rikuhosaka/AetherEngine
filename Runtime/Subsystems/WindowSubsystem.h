#pragma once

#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/Platform/GameWindow.h"

#include <memory>

class WindowSubsystem final : public ISubsystem
{
public:
	explicit WindowSubsystem(EngineLoopConfig config);

	[[nodiscard]] const char* GetName() const override { return "Window"; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Shutdown(SubsystemContext& ctx) override;

	[[nodiscard]] bool ProcessMessages();
	[[nodiscard]] bool HasPendingResize() const;
	void ClearPendingResize();
	[[nodiscard]] uint32_t GetPendingWidth() const;
	[[nodiscard]] uint32_t GetPendingHeight() const;

	[[nodiscard]] const WindowServices& GetServices() const noexcept { return m_services; }

private:
	void SyncServicesFromWindow();

	EngineLoopConfig m_config{};
	std::unique_ptr<GameWindow> m_window{};
	WindowServices m_services{};
};
