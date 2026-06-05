#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"

#include <filesystem>

[[nodiscard]] Result<ShaderBytecode> LoadShaderBytecodeFromFile(
	const std::filesystem::path& filePath);
