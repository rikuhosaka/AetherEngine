#pragma once

#include "Engine/Renderer/ShaderSystem/ShaderId.h"
#include "Engine/Renderer/ShaderSystem/ShaderMacro.h"
#include "Engine/Renderer/ShaderSystem/ShaderPermutationHash.h"

#include <span>

class IShaderCatalog
{
public:
	virtual ~IShaderCatalog() = default;

	[[nodiscard]] virtual bool IsValidPermutation(ShaderId id, ShaderPermutationHash permutation) const = 0;

	[[nodiscard]] virtual ShaderPermutationHash BuildPermutationHash(ShaderId id, std::span<const ShaderMacro> defines) const = 0;
};
