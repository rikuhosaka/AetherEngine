#include "Engine/Renderer/ShaderSystem/Cache/ShaderCacheHash.h"

#include "Engine/Core/Hash/HashCombine.h"

#include <algorithm>
#include <fstream>

namespace
{
	[[nodiscard]] std::uint64_t Fnv1a64(std::uint64_t hash, std::byte byte)
	{
		hash ^= static_cast<std::uint64_t>(byte);
		return hash * 1099511628211ULL;
	}

	[[nodiscard]] std::uint64_t Fnv1a64(std::uint64_t hash, std::span<const std::byte> bytes)
	{
		for (const std::byte byte : bytes)
		{
			hash = Fnv1a64(hash, byte);
		}
		return hash;
	}

	[[nodiscard]] std::uint64_t Fnv1a64(std::string_view text)
	{
		std::uint64_t hash = 14695981039346656037ULL;
		for (const char ch : text)
		{
			hash = Fnv1a64(hash, static_cast<std::byte>(ch));
		}
		return hash;
	}
}

std::uint64_t HashFileContentFingerprint(const std::filesystem::path& filePath)
{
	if (filePath.empty())
	{
		return 0;
	}

	std::ifstream stream(filePath, std::ios::binary);
	if (!stream)
	{
		return 0;
	}

	stream.seekg(0, std::ios::end);
	const std::streamoff size = stream.tellg();
	if (size <= 0)
	{
		return 0;
	}

	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	stream.seekg(0, std::ios::beg);
	stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if (!stream)
	{
		return 0;
	}

	return Fnv1a64(14695981039346656037ULL, std::span<const std::byte>(bytes));
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
	return Fnv1a64(14695981039346656037ULL, std::span<const std::byte>(bytecode.Data));
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
