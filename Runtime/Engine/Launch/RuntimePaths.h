#pragma once

#include <filesystem>

[[nodiscard]] std::filesystem::path ResolveExecutableDirectory();

[[nodiscard]] std::filesystem::path ResolveAssetsDirectory();

[[nodiscard]] std::filesystem::path ResolveCompiledShaderRoot();

[[nodiscard]] std::filesystem::path ResolveShaderRoot();

[[nodiscard]] std::filesystem::path ResolveEngineLogPath();

[[nodiscard]] std::filesystem::path ResolveDx12DebugConfigPath();

[[nodiscard]] std::filesystem::path ResolveConfigRoot(); 