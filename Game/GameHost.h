#pragma once

#include "Game/IGameHost.h"

class SubsystemContext;

class GameHost final : public IGameHost
{
public:
	explicit GameHost(SubsystemContext& context);

	[[nodiscard]] SubsystemContext& GetContext() override;
	[[nodiscard]] InputManager& GetInput() override;
	[[nodiscard]] float GetDeltaSeconds() const override;

private:
	SubsystemContext& m_context;
};
