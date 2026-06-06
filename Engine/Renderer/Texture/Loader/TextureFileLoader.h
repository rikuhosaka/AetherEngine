#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Texture/Loader/TextureLoadTypes.h"

#include <filesystem>

struct TextureFileLoadOptions
{
	TextureColorSpace colorSpace = TextureColorSpace::Auto;
	bool generateMips = true;
};

[[nodiscard]] Result<TextureImageData> LoadTextureImageFromFile(
	const std::filesystem::path& absolutePath,
	const TextureFileLoadOptions& options = {});
