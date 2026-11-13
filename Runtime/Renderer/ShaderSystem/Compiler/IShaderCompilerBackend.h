#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"
#include "Runtime/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"

class IShaderCompilerBackend
{
public:
	virtual ~IShaderCompilerBackend() = default;

	virtual Result<ShaderBytecode> Compile(const ShaderCompileDesc& desc) = 0;
};
