#pragma once

#include <filesystem>

class InputManager;
class RenderServices;
class SubsystemContext;

class IGameHost
{
public:
	virtual ~IGameHost() = default;

	[[nodiscard]] virtual SubsystemContext& GetContext() = 0;
	[[nodiscard]] virtual InputManager& GetInput() = 0;
	[[nodiscard]] virtual float GetDeltaSeconds() const = 0;
	[[nodiscard]] virtual RenderServices* GetRenderServices() = 0;
	[[nodiscard]] virtual std::filesystem::path GetShaderRoot() const = 0;
};
