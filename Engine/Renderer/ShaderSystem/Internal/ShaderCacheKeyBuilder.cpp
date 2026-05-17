#include "Engine/Renderer/ShaderSystem/Internal/ShaderCacheKeyBuilder.h"

#include "Engine/Renderer/ShaderSystem/Internal/ShaderHash.h"

#include <fstream>

namespace
{
	[[nodiscard]] std::optional<std::vector<std::byte>> ReadFileBytes(const std::filesystem::path& path)
	{
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
		{
			return std::nullopt;
		}

		stream.seekg(0, std::ios::end);
		const std::streamoff size = stream.tellg();
		if (size < 0)
		{
			return std::nullopt;
		}

		std::vector<std::byte> bytes(static_cast<std::size_t>(size));
		stream.seekg(0, std::ios::beg);
		stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		if (!stream)
		{
			return std::nullopt;
		}

		return bytes;
	}

	[[nodiscard]] std::uint64_t HashFileContents(const std::filesystem::path& path)
	{
		const std::optional<std::vector<std::byte>> bytes = ReadFileBytes(path);
		if (!bytes.has_value())
		{
			return 0;
		}
		return ShaderHash::Fnv1a64(ShaderHash::FnvOffsetBasis, std::span<const std::byte>(*bytes));
	}

	[[nodiscard]] std::uint32_t PackCompilerOptions(const ShaderCompileJob& job)
	{
		std::uint32_t packed = static_cast<std::uint32_t>(job.Stage);
		packed |= static_cast<std::uint32_t>(ShaderHash::Fnv1a64(job.Profile) & 0x00FFFFFFU) << 8;
		return packed;
	}

	[[nodiscard]] std::uint64_t HashDefines(std::span<const std::pair<std::string, std::string>> defines)
	{
		std::uint64_t hash = ShaderHash::FnvOffsetBasis;
		std::vector<std::pair<std::string, std::string>> sortedDefines(defines.begin(), defines.end());
		std::ranges::sort(sortedDefines, [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
		for (const auto& [name, value] : sortedDefines)
		{
			hash = ShaderHash::Fnv1a64(hash, name);
			hash = ShaderHash::Fnv1a64(hash, value);
		}
		return hash;
	}
}

ShaderCacheKeyBuilder::ShaderCacheKeyBuilder(ShaderCacheKeyBuilderOptions options)
	: m_options(std::move(options))
{
}

ShaderCacheKey ShaderCacheKeyBuilder::Build(const ShaderCompileJob& job) const
{
	const std::filesystem::path sourcePath = std::filesystem::path(job.SourcePath).is_absolute()
		? std::filesystem::path(job.SourcePath)
		: m_options.AssetsRoot / job.SourcePath;

	ShaderCacheKey key{};
	key.SourceContentFingerprint = HashFileContents(sourcePath);
	key.IncludeClosureFingerprint = key.SourceContentFingerprint;
	key.PermutationValue = HashDefines(job.Defines);
	key.CompilerIdentityToken = static_cast<std::uint32_t>(ShaderHash::Fnv1a64(m_options.CompilerIdentity));
	key.CompilerOptionsPacked = PackCompilerOptions(job);
	key.PipelineSpecToken = static_cast<std::uint32_t>(ShaderHash::Fnv1a64(job.EntryPoint));
	return key;
}
