#pragma once

#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionResult.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"

class IShaderReflectionBackend
{
public:
	virtual ~IShaderReflectionBackend() = default;

	[[nodiscard]] virtual ShaderReflectionResult Reflect(
		const ShaderBytecode& bytecode,
		ShaderStage stage) = 0;
};
