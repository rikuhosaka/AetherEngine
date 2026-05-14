#pragma once

#include <cstddef>
#include <span>

struct ShaderBytecodeView
{
	std::span<const std::byte> Bytes{};
};
