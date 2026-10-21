#pragma once

#include "Engine/AssetSystem/Texture/Loader/TextureLoadTypes.h"
#include "Engine/Renderer/Texture/TextureTypes.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

struct TextureLoadCacheKey
{
	std::filesystem::path path{};
	TextureColorSpace colorSpace = TextureColorSpace::Auto;
	bool generateMips = true;

	[[nodiscard]] bool operator==(const TextureLoadCacheKey& other) const
	{
		return generateMips == other.generateMips
			&& colorSpace == other.colorSpace
			&& path == other.path;
	}
};

struct TextureLoadCacheKeyHash
{
	[[nodiscard]] std::size_t operator()(const TextureLoadCacheKey& key) const;
};

class TextureLoadCache
{
public:
	[[nodiscard]] TextureHandle Find(const TextureLoadCacheKey& key) const;
	void Store(const TextureLoadCacheKey& key, TextureHandle handle);
	void Clear();

private:
	std::unordered_map<TextureLoadCacheKey, TextureHandle, TextureLoadCacheKeyHash> m_lookup{};
};
