#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"

class IShaderCompilerBackend;

class ShaderCompiler
{
public:
	explicit ShaderCompiler(std::unique_ptr<IShaderCompilerBackend> backend);
	~ShaderCompiler();

	Result<ShaderBytecode> Compile(const ShaderCompileDesc& desc);

private:
	std::unique_ptr<IShaderCompilerBackend> m_backend;
};
