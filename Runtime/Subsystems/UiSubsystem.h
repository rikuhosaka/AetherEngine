#pragma once

#include "Engine/Application/Services/UiServices.h"
#include "Engine/Application/Services/WorldSelectionServices.h"
#include "Engine/Application/Subsystem/ISubsystem.h"

#include <memory>

class RHIImGui;

class UiSubsystem final : public ISubsystem
{
public:
	UiSubsystem();
	~UiSubsystem() override;

	[[nodiscard]] const char* GetName() const override { return "UI"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] SubsystemTickGroup GetTickGroup() const override { return SubsystemTickGroup::Ui; }
	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	void Shutdown(SubsystemContext& ctx) override;

private:
	std::unique_ptr<RHIImGui> m_imgui{};
	UiServices m_services{};
	WorldSelectionServices m_selection{};
};

std::unique_ptr<ISubsystem> CreateUiSubsystem();
