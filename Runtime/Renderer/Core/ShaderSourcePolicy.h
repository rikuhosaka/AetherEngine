#pragma once

#include <cstdint>

enum class ShaderSourcePolicy : uint8_t
{
	PreferSource,
	PreferPrecompiled,
	SourceOnly,
	PrecompiledOnly,
};
