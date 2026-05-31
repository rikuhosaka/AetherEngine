#pragma once

#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Application/Subsystem/IEngineLoopPlatform.h"
#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/Platform/GameWindow.h"

#include <memory>

class WindowSubsystem final : public ISubsystem, public IEngineLoopPlatform
{
public:
	explicit WindowSubsystem(EngineLoopConfig config);

	[[nodiscard]] const char* GetName() const override { return "Window"; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Shutdown(SubsystemContext& ctx) override;

	bool ProcessPlatformMessages() override;
	[[nodiscard]] bool HasPendingResize() const override;
	void ClearPendingResize() override;
	[[nodiscard]] uint32_t GetPendingWidth() const override;
	[[nodiscard]] uint32_t GetPendingHeight() const override;

	[[nodiscard]] bool ProcessMessages();
	[[nodiscard]] const WindowServices& GetServices() const noexcept { return m_services; }

private:
	void SyncServicesFromWindow();

	EngineLoopConfig m_config{};
	std::unique_ptr<GameWindow> m_window{};
	WindowServices m_services{};
};
