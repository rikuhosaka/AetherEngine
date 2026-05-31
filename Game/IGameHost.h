#pragma once

class InputManager;
class SubsystemContext;

class IGameHost
{
public:
	virtual ~IGameHost() = default;

	[[nodiscard]] virtual SubsystemContext& GetContext() = 0;
	[[nodiscard]] virtual InputManager& GetInput() = 0;
	[[nodiscard]] virtual float GetDeltaSeconds() const = 0;
};
