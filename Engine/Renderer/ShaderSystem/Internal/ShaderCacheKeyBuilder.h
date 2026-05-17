#pragma once

#include "Engine/Renderer/ShaderSystem/Internal/ShaderCacheKey.h"
#include "Engine/Renderer/ShaderSystem/Internal/ShaderCompileJob.h"

#include <filesystem>

struct ShaderCacheKeyBuilderOptions
{
	std::filesystem::path AssetsRoot{};
	std::string CompilerIdentity{};
};

class ShaderCacheKeyBuilder
{
public:
	explicit ShaderCacheKeyBuilder(ShaderCacheKeyBuilderOptions options);

	[[nodiscard]] ShaderCacheKey Build(const ShaderCompileJob& job) const;

private:
	ShaderCacheKeyBuilderOptions m_options{};
};
