#pragma once

#include "Engine/Core/Log/Result.h"

#include <filesystem>

[[nodiscard]] Result<std::filesystem::path> ResolveTextureAssetPath(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath);
