#pragma once

#include "Engine/Application/Services/InputServices.h"
#include "Engine/Application/Subsystem/ISubsystem.h"

class InputSubsystem final : public ISubsystem
{
public:
	[[nodiscard]] const char* GetName() const override { return "Input"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] SubsystemTickGroup GetTickGroup() const override { return SubsystemTickGroup::Input; }
	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	Result<void> OnResize(SubsystemContext& ctx, uint32_t width, uint32_t height) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	void Shutdown(SubsystemContext& ctx) override;

private:
	InputServices m_services{};
};

std::unique_ptr<ISubsystem> CreateInputSubsystem();
