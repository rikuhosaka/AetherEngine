#pragma once

#include "Engine/Application/Services/DisplayServices.h"
#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Application/Subsystem/ISubsystem.h"

#include <memory>

class DisplaySubsystemImpl;

class DisplaySubsystem final : public ISubsystem
{
public:
	explicit DisplaySubsystem(EngineLoopConfig config);
	~DisplaySubsystem() override;

	[[nodiscard]] const char* GetName() const override { return "Display"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	Result<void> Initialize(SubsystemContext& ctx) override;
	Result<void> OnResize(SubsystemContext& ctx, uint32_t width, uint32_t height) override;
	void Shutdown(SubsystemContext& ctx) override;

	[[nodiscard]] const DisplayServices& GetServices() const noexcept { return m_services; }

private:
	EngineLoopConfig m_config{};
	std::unique_ptr<DisplaySubsystemImpl> m_impl{};
	DisplayServices m_services{};
};

std::unique_ptr<ISubsystem> CreateDisplaySubsystem(const EngineLoopConfig& config);
