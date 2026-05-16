#pragma once

#include "Engine/Renderer/ShaderSystem/ShaderCompileDiagnostics.h"

#include <cstddef>
#include <vector>

struct ShaderCompileOutput
{
	std::vector<std::byte> Bytecode{};
	ShaderCompileDiagnostics Diagnostics{};
};
