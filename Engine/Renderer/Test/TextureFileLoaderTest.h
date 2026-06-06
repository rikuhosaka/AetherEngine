#pragma once

#include "Engine/Core/Log/Result.h"

#include <filesystem>

[[nodiscard]] Result<void> RunTextureFileLoaderTests(const std::filesystem::path& assetsRoot);
