#pragma once

#include "Runtime/Core/Log/Result.h"

#include <filesystem>

[[nodiscard]] Result<void> RunSceneDocumentLoadTests(const std::filesystem::path& assetsRoot);
