#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompiler.h"

#include "Engine/Renderer/ShaderSystem/Compiler/IShaderCompilerBackend.h"

ShaderCompiler::ShaderCompiler(std::unique_ptr<IShaderCompilerBackend> backend)
	: m_backend(std::move(backend))
{
}

ShaderCompiler::~ShaderCompiler()
{
	m_backend.reset();
}

Result<ShaderBytecode> ShaderCompiler::Compile(const ShaderCompileDesc& desc)
{
	if (m_backend == nullptr)
	{
		return MakeFail<ShaderBytecode>(
			ErrorCode::InvalidArgument,
			"Shader compiler backend is not initialized.");
	}

	return m_backend->Compile(desc);
}
