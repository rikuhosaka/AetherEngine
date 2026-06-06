#pragma once

#include "Engine/Renderer/Texture/TextureTypes.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

class TextureLoadCache
{
public:
	[[nodiscard]] TextureHandle Find(const std::filesystem::path& normalizedAbsolutePath) const;
	void Store(const std::filesystem::path& normalizedAbsolutePath, TextureHandle handle);
	void Clear();

private:
	[[nodiscard]] static std::uint64_t HashPath(const std::filesystem::path& normalizedAbsolutePath);

	std::unordered_map<std::uint64_t, TextureHandle> m_lookup{};
};
