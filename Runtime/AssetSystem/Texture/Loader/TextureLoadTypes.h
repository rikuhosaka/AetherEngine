#pragma once

#include "Runtime/AssetData/TextureData.h"

#include <filesystem>

struct TextureLoadDesc
{
	// Assets/ relative path (used by TextureSystemServices in a later phase).
	std::filesystem::path relativePath{};

	TextureColorSpace colorSpace = TextureColorSpace::Auto;
	bool generateMips = true;

	const char* debugName = nullptr;
};
