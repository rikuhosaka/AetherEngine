#pragma once

struct ShaderPermutationHash
{
	std::uint64_t Value{};

	[[nodiscard]] friend constexpr auto operator<=>(ShaderPermutationHash, ShaderPermutationHash) = default;
};
