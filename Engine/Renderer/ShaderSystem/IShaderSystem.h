#pragma once

#include "Engine/Renderer/ShaderSystem/ShaderId.h"
#include "Engine/Renderer/ShaderSystem/ShaderPermutationHash.h"
#include "Engine/Renderer/ShaderSystem/ShaderProgramBundle.h"

#include <optional>

class IShaderSystem
{
public:
	virtual ~IShaderSystem() = default;

	[[nodiscard]] virtual std::optional<ShaderProgramBundle> TryGetProgram(ShaderId id, ShaderPermutationHash permutation) = 0;

	virtual void RequestProgram(ShaderId id, ShaderPermutationHash permutation) = 0;
};
