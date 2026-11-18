#include "Runtime/Renderer/Texture/Cache/TextureLoadCache.h"

#include "Runtime/Core/Hash/Fnv.h"

std::size_t TextureLoadCacheKeyHash::operator()(const TextureLoadCacheKey& key) const
{
	std::uint64_t hash = kFnv1a64OffsetBasis;
	hash = Fnv1a64Append(hash, key.path.generic_string());
	hash = Fnv1a64Append(hash, static_cast<uint8_t>(key.colorSpace));
	hash = Fnv1a64Append(hash, key.generateMips ? uint8_t{ 1 } : uint8_t{ 0 });
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
