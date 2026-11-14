#pragma once

#include "Runtime/Engine/Application/Subsystem/ISubsystem.h"

#include <memory>

// ImGui の実体は RenderSystem が持つ。ここにあるのは、そのフレームで UI を描けるかと入力キャプチャの結果だけ。
struct UiFrameCapture
{
	bool wantCaptureMouse = false;
	bool wantCaptureKeyboard = false;
	bool anyWindowHovered = false;
};

struct UiServices
{
	bool frameStarted = false;
	bool wantCaptureMouse = false;
	bool wantCaptureKeyboard = false;
	bool anyWindowHovered = false;
};

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

	[[nodiscard]] UiServices& GetUi() noexcept { return m_services; }
	[[nodiscard]] const UiServices& GetUi() const noexcept { return m_services; }

private:
	UiServices m_services{};
};

class UiPresentSubsystem final : public ISubsystem
{
public:
	[[nodiscard]] const char* GetName() const override { return "UiPresent"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] SubsystemTickGroup GetTickGroup() const override { return SubsystemTickGroup::UiPresent; }
	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	void Shutdown(SubsystemContext& ctx) override;
};

std::unique_ptr<ISubsystem> CreateUiSubsystem();
std::unique_ptr<ISubsystem> CreateUiPresentSubsystem();
