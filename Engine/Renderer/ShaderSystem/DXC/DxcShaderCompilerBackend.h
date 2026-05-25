#pragma once

#include "Engine/Renderer/ShaderSystem/Compiler/IShaderCompilerBackend.h"

class DxcShaderContext;

class DxcShaderCompilerBackend : public IShaderCompilerBackend
{
public:
	explicit DxcShaderCompilerBackend(DxcShaderContext* context);

	ShaderCompileResult Compile(const ShaderCompileDesc& desc) override;

private:
	DxcShaderContext* m_context{};
};
