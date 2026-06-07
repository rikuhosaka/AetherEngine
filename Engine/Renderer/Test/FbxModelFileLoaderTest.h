#pragma once

#include "Engine/Core/Log/Result.h"

#include <filesystem>

[[nodiscard]] Result<void> RunFbxModelFileLoaderTests(const std::filesystem::path& assetsRoot);
