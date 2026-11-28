#pragma once

#include "Runtime/Renderer/ShaderSystem/Reflection/ShaderReflectionTypes.h"
#include "Runtime/RHI/Common/RHIRootSignatureLayout.h"

struct ShaderRootBindingSlot
{
	std::string Name{};
	ShaderResourceType ShaderType = ShaderResourceType::Unknown;
	RHIRootParamType RootType = RHIRootParamType::CBV;

	std::uint32_t Register = 0;
	std::uint32_t Space = 0;
	std::uint32_t BindCount = 1;

	std::uint32_t RootParameterIndex = 0;
	std::uint32_t RangeIndex = 0;
	std::uint32_t TableOffset = 0;

	RHIShaderVisibility Visibility = RHIShaderVisibility::All;
	bool IsRootConstants = false;
};
