#pragma once

#include <filesystem>

struct ShaderSystemSettings
{
	std::filesystem::path AssetsRoot{};
	std::filesystem::path DiskCacheDirectory{};
	std::filesystem::path CompiledShadersDirectory{};
	std::filesystem::path DxcExecutable{};
	std::filesystem::path ScratchDirectory{};
};
