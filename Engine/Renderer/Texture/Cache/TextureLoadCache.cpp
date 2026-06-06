#include "Engine/Renderer/Texture/Cache/TextureLoadCache.h"

#include <string>

namespace
{
[[nodiscard]] std::uint64_t Fnv1a64(std::uint64_t hash, unsigned char byte)
{
	hash ^= static_cast<std::uint64_t>(byte);
	return hash * 1099511628211ULL;
}

[[nodiscard]] std::uint64_t Fnv1a64(std::string_view text)
{
	std::uint64_t hash = 14695981039346656037ULL;
	for (const char ch : text)
	{
		hash = Fnv1a64(hash, static_cast<unsigned char>(ch));
	}
	return hash;
}
} // namespace

std::uint64_t TextureLoadCache::HashPath(const std::filesystem::path& normalizedAbsolutePath)
{
	return Fnv1a64(normalizedAbsolutePath.generic_string());
}

TextureHandle TextureLoadCache::Find(const std::filesystem::path& normalizedAbsolutePath) const
{
	if (normalizedAbsolutePath.empty())
	{
		return {};
	}

	const auto found = m_lookup.find(HashPath(normalizedAbsolutePath));
	if (found == m_lookup.end())
	{
		return {};
	}

	return found->second;
}

void TextureLoadCache::Store(
	const std::filesystem::path& normalizedAbsolutePath,
	TextureHandle handle)
{
	if (normalizedAbsolutePath.empty() || !handle.IsValid())
	{
		return;
	}

	m_lookup[HashPath(normalizedAbsolutePath)] = handle;
}

void TextureLoadCache::Clear()
{
	m_lookup.clear();
}
