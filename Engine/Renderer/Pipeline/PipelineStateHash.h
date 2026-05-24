#pragma once

#include "Engine/Core/Hash/HashCombine.h"
#include "Engine/RHI/Common/RHIPipelineStateLayout.h"
#include "Engine/Renderer/Pipeline/RootSignatureHash.h"

[[nodiscard]] inline std::uint64_t HashPipelineStateLayout(const RHIPipelineStateLayout& layout)
{
	std::size_t hash = 0;

	HashCombine(hash, reinterpret_cast<std::size_t>(layout.vertexShader));
	HashCombine(hash, reinterpret_cast<std::size_t>(layout.pixelShader));

	HashCombine(hash, static_cast<std::size_t>(layout.inputLayout));
	HashCombine(hash, static_cast<std::size_t>(layout.topology));
	HashCombine(hash, static_cast<std::size_t>(layout.raster));
	HashCombine(hash, static_cast<std::size_t>(layout.blend));
	HashCombine(hash, static_cast<std::size_t>(layout.depth));

	HashCombine(hash, static_cast<std::size_t>(layout.numRT));
	HashCombine(hash, static_cast<std::size_t>(layout.sampleCount));
	HashCombine(hash, static_cast<std::size_t>(layout.dsvFormat));

	for (uint8_t i = 0; i < layout.numRT; ++i)
	{
		HashCombine(hash, static_cast<std::size_t>(layout.rtvFormats[i]));
	}

	const std::uint64_t rootSignatureHash = HashRootSignatureLayout(layout.rootSignature);
	HashCombine(hash, static_cast<std::size_t>(rootSignatureHash));

	return static_cast<std::uint64_t>(hash);
}
