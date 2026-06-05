#pragma once

#include "Engine/Application/Services/RHIServices.h"
#include "Engine/Application/Subsystem/EngineLoopConfig.h"
#include "Engine/Application/Subsystem/ISubsystem.h"

#include <memory>

class RHISubsystemImpl;

class RHISubsystem final : public ISubsystem
{
public:
	explicit RHISubsystem(EngineLoopConfig config);
	~RHISubsystem() override;

	[[nodiscard]] const char* GetName() const override { return "RHI"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Shutdown(SubsystemContext& ctx) override;

	[[nodiscard]] const RHIServices& GetServices() const noexcept { return m_services; }

private:
	Result<void> CreateFrameResources();
	void WaitForPendingFrames();

	std::unique_ptr<RHISubsystemImpl> m_impl{};
	RHIServices m_services{};
	EngineLoopConfig m_config{};
};

std::unique_ptr<ISubsystem> CreateRHISubsystem(const EngineLoopConfig& config);
