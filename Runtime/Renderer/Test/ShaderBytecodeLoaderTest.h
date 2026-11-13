#pragma once

#include "Runtime/Core/Log/Result.h"

#include <filesystem>

[[nodiscard]] Result<void> RunShaderBytecodeLoaderTests(
	const std::filesystem::path& compiledShaderRoot);
