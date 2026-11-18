#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

inline constexpr uint64_t kFnv1a64OffsetBasis = 14695981039346656037ull;
inline constexpr uint64_t kFnv1a64Prime = 1099511628211ull;

[[nodiscard]] inline uint64_t Fnv1a64Append(uint64_t hash, uint8_t byte)
{
	hash ^= byte;
	hash *= kFnv1a64Prime;
	return hash;
}

[[nodiscard]] inline uint64_t Fnv1a64Append(uint64_t hash, std::string_view bytes)
{
	for (const char character : bytes)
	{
		hash = Fnv1a64Append(hash, static_cast<uint8_t>(character));
	}

	return hash;
}

[[nodiscard]] inline uint64_t Fnv1a64Append(uint64_t hash, std::span<const std::byte> bytes)
{
	for (const std::byte byte : bytes)
	{
		hash = Fnv1a64Append(hash, static_cast<uint8_t>(byte));
	}

	return hash;
}

[[nodiscard]] inline uint64_t Fnv1a64(std::string_view bytes)
{
	return Fnv1a64Append(kFnv1a64OffsetBasis, bytes);
}

[[nodiscard]] inline uint64_t Fnv1a64(std::span<const std::byte> bytes)
{
	return Fnv1a64Append(kFnv1a64OffsetBasis, bytes);
}
