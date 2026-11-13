#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"

#include <filesystem>

[[nodiscard]] Result<ShaderBytecode> LoadShaderBytecodeFromFile(
	const std::filesystem::path& filePath);
