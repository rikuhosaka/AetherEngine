#pragma once

#include "Engine/Renderer/ShaderSystem/Cache/ShaderCacheTypes.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"

#include <cstdint>
#include <filesystem>

[[nodiscard]] std::uint64_t HashFileContentFingerprint(const std::filesystem::path& filePath);

[[nodiscard]] std::uint64_t HashShaderCompileDesc(const ShaderCompileDesc& desc);

[[nodiscard]] ShaderCompileCacheKey BuildShaderCompileCacheKey(const ShaderCompileDesc& desc);

[[nodiscard]] std::uint64_t HashShaderCompileCacheLookupKey(const ShaderCompileCacheKey& key);

[[nodiscard]] std::uint64_t HashBytecodeFingerprint(const ShaderBytecode& bytecode);

[[nodiscard]] ShaderReflectionCacheKey BuildShaderReflectionCacheKey(
	const ShaderBytecode& bytecode,
	ShaderStage stage);

[[nodiscard]] std::uint64_t HashShaderReflectionCacheLookupKey(const ShaderReflectionCacheKey& key);
