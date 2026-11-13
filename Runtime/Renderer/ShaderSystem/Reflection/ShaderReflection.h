#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"
#include "Runtime/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"
#include "Runtime/Renderer/ShaderSystem/Compiler/ShaderType.h"

class IShaderReflectionBackend;

class ShaderReflection
{
public:
	explicit ShaderReflection(std::unique_ptr<IShaderReflectionBackend> backend);

	[[nodiscard]] Result<ShaderReflectionData> Reflect(
		const ShaderBytecode& bytecode,
		ShaderStage stage) const;

private:
	std::unique_ptr<IShaderReflectionBackend> m_backend;
};
