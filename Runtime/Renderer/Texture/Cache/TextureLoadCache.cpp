#include "Runtime/Renderer/Texture/Cache/TextureLoadCache.h"

TextureHandle TextureLoadCache::Find(std::string_view key) const
{
	if (key.empty())
	{
		return {};
	}

	const auto found = m_lookup.find(std::string{ key });
	if (found == m_lookup.end())
	{
		return {};
	}

	return found->second;
}

void TextureLoadCache::Store(std::string_view key, TextureHandle handle)
{
	if (key.empty() || !handle.IsValid())
	{
		return;
	}

	m_lookup[std::string{ key }] = handle;
}

void TextureLoadCache::Clear()
{
	m_lookup.clear();
}
