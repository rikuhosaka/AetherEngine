#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"

class IShaderCompilerBackend
{
public:
	virtual ~IShaderCompilerBackend() = default;

	virtual Result<ShaderBytecode> Compile(const ShaderCompileDesc& desc) = 0;
};
