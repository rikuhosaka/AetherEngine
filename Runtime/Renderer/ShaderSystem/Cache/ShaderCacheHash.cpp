#include "Runtime/Renderer/ShaderSystem/Cache/ShaderCacheHash.h"

#include "Runtime/Core/Hash/Fnv.h"
#include "Runtime/Core/Hash/HashCombine.h"
#include "Runtime/Core/IO/BinaryFile.h"

#include <algorithm>

std::uint64_t HashFileContentFingerprint(const std::filesystem::path& filePath)
{
	if (filePath.empty())
	{
		return 0;
	}

	const Result<std::string> bytes = ReadBinaryFile(filePath);
	if (!bytes || bytes.value.empty())
	{
		return 0;
	}

	return Fnv1a64(bytes.value);
}

std::uint64_t HashShaderCompileDesc(const ShaderCompileDesc& desc)
{
	std::size_t hash = 0;

	HashCombine(hash, Fnv1a64(desc.FilePath.string()));
	HashCombine(hash, Fnv1a64(desc.EntryPoint));
	HashCombine(hash, static_cast<std::size_t>(desc.Stage));
	HashCombine(hash, static_cast<std::size_t>(desc.Model));
	HashCombine(hash, static_cast<std::size_t>(desc.Debug));
	HashCombine(hash, static_cast<std::size_t>(desc.Optimization));
	HashCombine(hash, static_cast<std::size_t>(desc.TreatWarningsAsErrors));

	std::vector<ShaderDefine> sortedDefines = desc.Defines;
	std::ranges::sort(sortedDefines, [](const ShaderDefine& lhs, const ShaderDefine& rhs) {
		return lhs.Name < rhs.Name;
	});
	HashCombine(hash, sortedDefines.size());
	for (const ShaderDefine& define : sortedDefines)
	{
		HashCombine(hash, Fnv1a64(define.Name));
		HashCombine(hash, Fnv1a64(define.Value));
	}

	std::vector<std::filesystem::path> sortedIncludes = desc.IncludeDirectories;
	std::ranges::sort(sortedIncludes);
	HashCombine(hash, sortedIncludes.size());
	for (const std::filesystem::path& includeDirectory : sortedIncludes)
	{
		HashCombine(hash, Fnv1a64(includeDirectory.string()));
	}

	return static_cast<std::uint64_t>(hash);
}

ShaderCompileCacheKey BuildShaderCompileCacheKey(const ShaderCompileDesc& desc)
{
	return ShaderCompileCacheKey{
		.SourceFingerprint = HashFileContentFingerprint(desc.FilePath),
		.DescFingerprint = HashShaderCompileDesc(desc),
	};
}

std::uint64_t HashShaderCompileCacheLookupKey(const ShaderCompileCacheKey& key)
{
	std::size_t hash = 0;
	HashCombine(hash, key.SourceFingerprint);
	HashCombine(hash, key.DescFingerprint);
	return static_cast<std::uint64_t>(hash);
}

std::uint64_t HashBytecodeFingerprint(const ShaderBytecode& bytecode)
{
	return Fnv1a64(std::span<const std::byte>(bytecode.Data));
}

ShaderReflectionCacheKey BuildShaderReflectionCacheKey(
	const ShaderBytecode& bytecode,
	ShaderStage stage)
{
	return ShaderReflectionCacheKey{
		.BytecodeFingerprint = HashBytecodeFingerprint(bytecode),
		.Stage = stage,
	};
}

std::uint64_t HashShaderReflectionCacheLookupKey(const ShaderReflectionCacheKey& key)
{
	std::size_t hash = 0;
	HashCombine(hash, key.BytecodeFingerprint);
	HashCombine(hash, static_cast<std::size_t>(key.Stage));
	return static_cast<std::uint64_t>(hash);
}
