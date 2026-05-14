#pragma once

#include <cstdint>

enum class ShaderStage : std::uint8_t
{
	Vertex,
	Pixel,
	Compute,

	Count,
};
