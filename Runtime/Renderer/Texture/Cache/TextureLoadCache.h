#pragma once

#include "Runtime/Renderer/Texture/TextureTypes.h"

#include <string>
#include <string_view>
#include <unordered_map>

class TextureLoadCache
{
public:
	[[nodiscard]] TextureHandle Find(std::string_view key) const;
	void Store(std::string_view key, TextureHandle handle);
	void Clear();

private:
	std::unordered_map<std::string, TextureHandle> m_lookup{};
};
