#pragma once

#include <Engine/Renderer/ShaderSystem/Internal/ShaderCompileJob.h>
#include <Engine/Renderer/ShaderSystem/Internal/ShaderCompileOutput.h>

class IShaderCompilerBackend
{
public:
	virtual ~IShaderCompilerBackend() = default;

	[[nodiscard]] virtual bool Compile(const ShaderCompileJob& job, ShaderCompileOutput& out) = 0;
};
