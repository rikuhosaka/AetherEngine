#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Pipeline/ShaderRootLayoutTypes.h"
#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"

class ShaderRootLayoutBuilder
{
public:
	void Clear();
	void AddStage(const ShaderReflectionData& reflection);
	void SetOptions(ShaderRootLayoutBuildOptions options);

	[[nodiscard]] Result<ShaderRootLayoutData> Build() const;

private:
	std::vector<ShaderReflectionData> m_stages{};
	ShaderRootLayoutBuildOptions m_options{};
};

[[nodiscard]] Result<ShaderRootLayoutData> BuildRootSignatureLayout(
	std::span<const ShaderReflectionData> stages,
	ShaderRootLayoutBuildOptions options = {});
