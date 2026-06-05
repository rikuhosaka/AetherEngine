#pragma once

#include "Engine/Core/Log/Result.h"

#include <filesystem>

[[nodiscard]] Result<void> RunShaderBytecodeLoaderTests(
	const std::filesystem::path& compiledShaderRoot);
