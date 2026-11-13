#pragma once

#include "Runtime/Core/Log/Result.h"

#include <filesystem>

[[nodiscard]] Result<void> RunAssetCacheTests(const std::filesystem::path& assetsRoot);
