#pragma once

#include "Engine/Renderer/ShaderSystem/Internal/ShaderCacheKey.h"

class IShaderDiskCache
{
public:
	virtual ~IShaderDiskCache() = default;

	[[nodiscard]] virtual bool TryRead(const ShaderCacheKey& key, std::vector<std::byte>& outBytecode) const = 0;

	virtual bool Write(const ShaderCacheKey& key, std::span<const std::byte> bytecode) = 0;
};
