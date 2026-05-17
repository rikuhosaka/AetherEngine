#pragma once

#include <cstdint>
#include <string_view>

namespace ShaderHash
{
	inline constexpr std::uint64_t FnvOffsetBasis = 14695981039346656037ULL;
	inline constexpr std::uint64_t FnvPrime = 1099511628211ULL;

	[[nodiscard]] constexpr std::uint64_t Fnv1a64(std::uint64_t hash, std::byte byte) noexcept
	{
		hash ^= static_cast<std::uint64_t>(byte);
		return hash * FnvPrime;
	}

	[[nodiscard]] inline std::uint64_t Fnv1a64(std::uint64_t hash, std::string_view text) noexcept
	{
		for (const char ch : text)
		{
			hash = Fnv1a64(hash, static_cast<std::byte>(ch));
		}
		return hash;
	}

	[[nodiscard]] inline std::uint64_t Fnv1a64(std::string_view text) noexcept
	{
		return Fnv1a64(FnvOffsetBasis, text);
	}

	[[nodiscard]] inline std::uint64_t Fnv1a64(std::uint64_t hash, std::span<const std::byte> bytes) noexcept
	{
		for (const std::byte byte : bytes)
		{
			hash = Fnv1a64(hash, byte);
		}
		return hash;
	}
}
