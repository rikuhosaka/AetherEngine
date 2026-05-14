#pragma once

#include <string_view>

struct ShaderMacro
{
	std::string_view Name{};
	std::string_view Value{};
};
