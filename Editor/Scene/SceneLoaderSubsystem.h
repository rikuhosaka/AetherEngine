#pragma once

#include "Editor/Camera/EditorCamera.h"

#include "Engine/Application/Services/EditorCameraServices.h"
#include "Engine/Application/Services/EditorSaveServices.h"
#include "Engine/Application/Subsystem/ISubsystem.h"

class SceneLoaderSubsystem final : public ISubsystem
{
public:
	SceneLoaderSubsystem() = default;

	[[nodiscard]] const char* GetName() const override { return "Editor"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	void Shutdown(SubsystemContext& ctx) override;

private:
	[[nodiscard]] Result<void> SaveEditorScene(SubsystemContext& ctx);

	EditorCamera m_camera{};
	EditorCameraServices m_cameraServices{};
	EditorSaveServices m_editorSave{};
};

std::unique_ptr<ISubsystem> CreateEditorSubsystem();
