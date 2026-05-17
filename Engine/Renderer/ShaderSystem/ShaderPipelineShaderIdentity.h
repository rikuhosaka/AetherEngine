#pragma once

#include "Engine/Renderer/ShaderSystem/ShaderId.h"
#include "Engine/Renderer/ShaderSystem/ShaderPermutationHash.h"

struct ShaderPipelineShaderIdentity
{
	ShaderId Shader{};
	ShaderPermutationHash Permutation{};

	[[nodiscard]] friend constexpr auto operator<=>(ShaderPipelineShaderIdentity, ShaderPipelineShaderIdentity) = default;
};
