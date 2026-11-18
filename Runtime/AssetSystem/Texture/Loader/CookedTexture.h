#pragma once

#include "Runtime/AssetSystem/Texture/Loader/TextureFileLoader.h"
#include "Runtime/Core/Log/Result.h"

#include <cstdint>
#include <filesystem>

// Bump when TextureImageData or the decode options change shape.
constexpr uint32_t kCookedTextureVersion = 1;

// Cooked file for one texture view. The same source can have separate files per color space and mip choice.
[[nodiscard]] std::filesystem::path CookedTexturePath(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	const TextureFileLoadOptions& options);

// False when the file is missing, stale, or incomplete.
[[nodiscard]] bool TryReadCookedTexture(
	const std::filesystem::path& path,
	uint64_t sourceHash,
	const TextureFileLoadOptions& options,
	const std::filesystem::path& relativePath,
	TextureImageData& outImage);

[[nodiscard]] Result<void> WriteCookedTexture(
	const std::filesystem::path& path,
	uint64_t sourceHash,
	const TextureFileLoadOptions& options,
	const std::filesystem::path& relativePath,
	const TextureImageData& image);

// Reads a cooked texture when the source bytes and decode options still match. Otherwise decodes and writes the cook.
[[nodiscard]] Result<TextureImageData> LoadTextureImage(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	const TextureFileLoadOptions& options = {});
