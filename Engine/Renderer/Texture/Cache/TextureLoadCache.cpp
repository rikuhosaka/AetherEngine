#include "Engine/Renderer/Texture/Cache/TextureLoadCache.h"

#include <string>

namespace
{
[[nodiscard]] std::uint64_t Fnv1a64(std::uint64_t hash, unsigned char byte)
{
	hash ^= static_cast<std::uint64_t>(byte);
	return hash * 1099511628211ULL;
}

[[nodiscard]] std::uint64_t Fnv1a64(std::uint64_t hash, std::string_view text)
{
	for (const char ch : text)
	{
		hash = Fnv1a64(hash, static_cast<unsigned char>(ch));
	}
	return hash;
}
} // namespace

std::size_t TextureLoadCacheKeyHash::operator()(const TextureLoadCacheKey& key) const
{
	std::uint64_t hash = 14695981039346656037ULL;
	hash = Fnv1a64(hash, key.path.generic_string());
	hash = Fnv1a64(hash, static_cast<unsigned char>(key.colorSpace));
	hash = Fnv1a64(hash, key.generateMips ? 1 : 0);
	return static_cast<std::size_t>(hash);
}

TextureHandle TextureLoadCache::Find(const TextureLoadCacheKey& key) const
{
	if (key.path.empty())
	{
		return {};
	}

	const auto found = m_lookup.find(key);
	if (found == m_lookup.end())
	{
		return {};
	}

	return found->second;
}

void TextureLoadCache::Store(const TextureLoadCacheKey& key, TextureHandle handle)
{
	if (key.path.empty() || !handle.IsValid())
	{
		return;
	}

	m_lookup[key] = handle;
}

void TextureLoadCache::Clear()
{
	m_lookup.clear();
}
