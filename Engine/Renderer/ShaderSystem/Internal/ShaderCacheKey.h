#pragma once

struct ShaderCacheKey
{
	std::uint64_t SourceContentFingerprint{};
	std::uint64_t IncludeClosureFingerprint{};
	std::uint64_t PermutationValue{};

	std::uint32_t CompilerIdentityToken{};
	std::uint32_t CompilerOptionsPacked{};

	std::uint32_t PipelineSpecToken{};
	std::uint32_t Reserved{};

	[[nodiscard]] friend constexpr bool operator==(ShaderCacheKey, ShaderCacheKey) = default;
};
