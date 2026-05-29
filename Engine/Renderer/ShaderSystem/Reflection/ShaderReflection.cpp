#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflection.h"

#include "Engine/Renderer/ShaderSystem/Reflection/IShaderReflectionBackend.h"

ShaderReflection::ShaderReflection(std::unique_ptr<IShaderReflectionBackend> backend)
	: m_backend(std::move(backend))
{
}

Result<ShaderReflectionData> ShaderReflection::Reflect(
	const ShaderBytecode& bytecode,
	ShaderStage stage) const
{
	if (m_backend == nullptr)
	{
		return MakeFail<ShaderReflectionData>(
			ErrorCode::InvalidArgument,
			"Shader reflection backend is not initialized.");
	}

	if (bytecode.Data.empty())
	{
		return MakeFail<ShaderReflectionData>(
			ErrorCode::InvalidArgument,
			"Shader bytecode is empty.");
	}

	auto result = m_backend->Reflect(bytecode, stage);
	if (!result)
	{
		return result;
	}

	result.value.Stage = stage;
	return result;
}
