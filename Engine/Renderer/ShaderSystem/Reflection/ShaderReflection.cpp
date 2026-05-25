#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflection.h"
#include "Engine/Renderer/ShaderSystem/Reflection/IShaderReflectionBackend.h"

ShaderReflection::ShaderReflection(std::unique_ptr<IShaderReflectionBackend> backend)
	: m_backend(std::move(backend))
{
}

ShaderReflectionResult ShaderReflection::Reflect(
	const ShaderBytecode& bytecode,
	ShaderStage stage) const
{
	if (m_backend == nullptr)
	{
		return ShaderReflectionResult{
			.Errors = "Shader reflection backend is not initialized.",
		};
	}

	if (bytecode.Data.empty())
	{
		return ShaderReflectionResult{
			.Errors = "Shader bytecode is empty.",
		};
	}

	ShaderReflectionResult result = m_backend->Reflect(bytecode, stage);
	if (!result.Succeeded)
	{
		LOG_ERROR(result.Errors);
		return result;
	}

	result.Data.Stage = stage;
	return result;
}