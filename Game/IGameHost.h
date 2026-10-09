#pragma once

#include <cstdint>
#include <filesystem>
#include <utility>

class InputState;
class RenderServices;
class SubsystemContext;

class IGameHost
{
public:
	virtual ~IGameHost() = default;

	[[nodiscard]] virtual SubsystemContext& GetContext() = 0;
	[[nodiscard]] virtual const InputState* GetInput() = 0;
	[[nodiscard]] virtual float GetDeltaSeconds() const = 0;
	[[nodiscard]] virtual RenderServices* GetRenderServices() = 0;
	[[nodiscard]] virtual std::filesystem::path GetShaderRoot() const = 0;
	[[nodiscard]] virtual std::pair<uint32_t, uint32_t> GetViewportSize() const = 0;
};
