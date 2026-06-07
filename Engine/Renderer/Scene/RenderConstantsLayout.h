#pragma once

#include <cstdint>

struct Material;

namespace RenderRegisters
{
constexpr uint32_t FrameConstants = 0;
constexpr uint32_t ObjectConstants = 1;
constexpr uint32_t MaterialConstants = 2;
} // namespace RenderRegisters

[[nodiscard]] bool MaterialUsesPassConstantBuffers(const Material& material);

[[nodiscard]] bool IsPassBoundConstantRegister(
	const Material& material,
	uint32_t registerIndex,
	uint32_t space);
