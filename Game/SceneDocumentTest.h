#pragma once

#include "Engine/Core/Log/Result.h"

#include <filesystem>

[[nodiscard]] Result<void> RunSceneDocumentTests(const std::filesystem::path& assetsRoot);
