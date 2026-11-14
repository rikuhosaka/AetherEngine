#pragma once

#include "Runtime/Engine/Application/Subsystem/ISubsystem.h"
#include "Runtime/Core/Platform/InputState.h"

#include <memory>

class Win32InputDevice;

struct InputServices
{
	const InputState* state = nullptr;
	bool relativeMouse = false;
	bool mouseCaptured = false;
	bool keyboardCaptured = false;
};

class InputSubsystem final : public ISubsystem
{
public:
	InputSubsystem();
	~InputSubsystem() override;

	[[nodiscard]] const char* GetName() const override { return "Input"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] SubsystemTickGroup GetTickGroup() const override { return SubsystemTickGroup::Input; }
	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	Result<void> OnResize(SubsystemContext& ctx, uint32_t width, uint32_t height) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	void Shutdown(SubsystemContext& ctx) override;

	[[nodiscard]] InputServices& GetInput() noexcept { return m_services; }
	[[nodiscard]] const InputServices& GetInput() const noexcept { return m_services; }

private:
	std::unique_ptr<Win32InputDevice> m_device;
	InputState m_state{};
	InputServices m_services{};
};

std::unique_ptr<ISubsystem> CreateInputSubsystem();
