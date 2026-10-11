#pragma once

#include "Engine/Core/Log/Result.h"

#include <filesystem>

[[nodiscard]] Result<void> RunDebugStartupTests(
	const std::filesystem::path& compiledShaderRoot,
	const std::filesystem::path& assetsRoot);
