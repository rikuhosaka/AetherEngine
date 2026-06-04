#pragma once

#include "Game/IGameHost.h"

#include <filesystem>

class SubsystemContext;

class GameHost final : public IGameHost
{
public:
	explicit GameHost(SubsystemContext& context);

	[[nodiscard]] SubsystemContext& GetContext() override;
	[[nodiscard]] InputManager& GetInput() override;
	[[nodiscard]] float GetDeltaSeconds() const override;
	[[nodiscard]] RenderServices* GetRenderServices() override;
	[[nodiscard]] std::filesystem::path GetShaderRoot() const override;

private:
	SubsystemContext& m_context;
};
