#pragma once

#include "Game/IGameHost.h"

#include <filesystem>

class SubsystemContext;

class GameHost final : public IGameHost
{
public:
	explicit GameHost(SubsystemContext& context);

	[[nodiscard]] SubsystemContext& GetContext() override;
	[[nodiscard]] const InputState* GetInput() override;
	void SetRelativeMouse(bool enabled) override;
	[[nodiscard]] float GetDeltaSeconds() const override;
	[[nodiscard]] RenderServices* GetRenderServices() override;
	[[nodiscard]] std::filesystem::path GetShaderRoot() const override;
	[[nodiscard]] std::pair<uint32_t, uint32_t> GetViewportSize() const override;

private:
	SubsystemContext& m_context;
};
