#pragma once

#include "Engine/Renderer/ShaderSystem/Reflection/IShaderReflectionBackend.h"

class DxcShaderContext;

class DxcShaderReflectionBackend final : public IShaderReflectionBackend
{
public:
	explicit DxcShaderReflectionBackend(DxcShaderContext* context);

	[[nodiscard]] Result<ShaderReflectionData> Reflect(
		const ShaderBytecode& bytecode,
		ShaderStage stage) override;

private:
	DxcShaderContext* m_context{};
};
