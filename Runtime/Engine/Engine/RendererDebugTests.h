#pragma once

#include "Runtime/Core/Log/Result.h"

#include <filesystem>

[[nodiscard]] Result<void> RunRendererDebugTests(
	const std::filesystem::path& compiledShaderRoot,
	const std::filesystem::path& assetsRoot);
