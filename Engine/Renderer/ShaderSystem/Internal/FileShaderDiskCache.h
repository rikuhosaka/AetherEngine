#pragma once

#include "Engine/Renderer/ShaderSystem/Internal/IShaderDiskCache.h"

#include <filesystem>

class FileShaderDiskCache final : public IShaderDiskCache
{
public:
	explicit FileShaderDiskCache(std::filesystem::path cacheRoot);

	[[nodiscard]] bool TryRead(const ShaderCacheKey& key, std::vector<std::byte>& outBytecode) const override;

	bool Write(const ShaderCacheKey& key, std::span<const std::byte> bytecode) override;

private:
	[[nodiscard]] std::filesystem::path BuildCachePath(const ShaderCacheKey& key) const;

	std::filesystem::path m_cacheRoot{};
};
