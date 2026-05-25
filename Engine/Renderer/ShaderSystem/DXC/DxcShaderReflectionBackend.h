#pragma once

#include "Engine/Renderer/ShaderSystem/Reflection/IShaderReflectionBackend.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"

class DxcShaderContext;

class DxcShaderReflectionBackend final : public IShaderReflectionBackend
{
public:
	explicit DxcShaderReflectionBackend(DxcShaderContext* context);

	[[nodiscard]] ShaderReflectionResult Reflect(
		const ShaderBytecode& bytecode,
		ShaderStage stage) override;

private:
	DxcShaderContext* m_context{};
};
