#pragma once

#include "Engine/Renderer/ShaderSystem/ShaderCompileDiagnostics.h"

struct ShaderCompileOutput
{
	std::vector<std::byte> Bytecode{};
	ShaderCompileDiagnostics Diagnostics{};
};
