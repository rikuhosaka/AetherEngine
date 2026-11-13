#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"
#include "Runtime/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"
#include "Runtime/Renderer/ShaderSystem/Compiler/ShaderType.h"

class IShaderReflectionBackend
{
public:
	virtual ~IShaderReflectionBackend() = default;

	[[nodiscard]] virtual Result<ShaderReflectionData> Reflect(
		const ShaderBytecode& bytecode,
		ShaderStage stage) = 0;
};
