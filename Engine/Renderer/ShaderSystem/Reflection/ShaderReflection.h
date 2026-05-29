#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"

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
