#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"

class IShaderReflectionBackend
{
public:
	virtual ~IShaderReflectionBackend() = default;

	[[nodiscard]] virtual Result<ShaderReflectionData> Reflect(
		const ShaderBytecode& bytecode,
		ShaderStage stage) = 0;
};
