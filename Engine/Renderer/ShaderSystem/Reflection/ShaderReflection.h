#pragma once

#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionResult.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"

class IShaderReflectionBackend;

class ShaderReflection
{
public:
	explicit ShaderReflection(std::unique_ptr<IShaderReflectionBackend> backend);

	[[nodiscard]] ShaderReflectionResult Reflect(
		const ShaderBytecode& bytecode,
		ShaderStage stage) const;

private:
	std::unique_ptr<IShaderReflectionBackend> m_backend;
};
