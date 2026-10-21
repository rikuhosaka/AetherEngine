#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

enum class TextureColorSpace : uint8_t
{
	Linear,
	Srgb,
	Auto,
};

enum class TextureImageFormat : uint8_t
{
	R8G8B8A8_UNORM,
	R8G8B8A8_UNORM_SRGB,
};

struct TextureImageMip
{
	std::vector<std::byte> pixels{};
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t rowPitch = 0;
};

struct TextureImageData
{
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t mipCount = 0;
	TextureImageFormat format = TextureImageFormat::R8G8B8A8_UNORM;
	std::vector<TextureImageMip> mips{};
	std::filesystem::path sourcePath{};
};

struct TextureLoadDesc
{
	// Assets/ relative path (used by TextureSystemServices in a later phase).
	std::filesystem::path relativePath{};

	TextureColorSpace colorSpace = TextureColorSpace::Auto;
	bool generateMips = true;

	const char* debugName = nullptr;
};
