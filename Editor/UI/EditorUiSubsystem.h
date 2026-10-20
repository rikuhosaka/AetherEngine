#pragma once

#include "Engine/Application/Services/WorldSelectionServices.h"
#include "Engine/Application/Subsystem/ISubsystem.h"

class EditorUiSubsystem final : public ISubsystem
{
public:
	[[nodiscard]] const char* GetName() const override { return "EditorUi"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] SubsystemTickGroup GetTickGroup() const override { return SubsystemTickGroup::EditorUi; }
	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	void Shutdown(SubsystemContext& ctx) override;

private:
	WorldSelectionServices m_selection{};
};

std::unique_ptr<ISubsystem> CreateEditorUiSubsystem();
