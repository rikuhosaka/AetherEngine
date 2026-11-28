#pragma once

#include "Runtime/RHI/Common/RHIRootSignatureLayout.h"

[[nodiscard]] inline RHIRootSignatureLayout MakeGraphicsRootSignatureLayout()
{
	RHIRootSignatureLayout layout{};
	layout.flags = RHIRootSignatureFlags::AllowInputAssembler;
	layout.DebugName = "Graphics";

	const auto addTable = [&](
		RHIRootParamType type,
		std::uint32_t shaderRegister,
		std::uint32_t count,
		RHIShaderVisibility visibility)
	{
		RHIRootParameterDesc parameter{};
		parameter.kind = RHIRootParameterKind::DescriptorTable;
		parameter.visibility = visibility;

		RHIRootDescriptorRange range{};
		range.type = type;
		range.count = count;
		range.baseRegister = shaderRegister;
		range.space = 0;
		range.offset = 0;
		parameter.ranges.push_back(range);
		layout.parameters.push_back(std::move(parameter));
	};

	addTable(RHIRootParamType::CBV, 0, 1, RHIShaderVisibility::All);
	addTable(RHIRootParamType::CBV, 1, 1, RHIShaderVisibility::All);
	addTable(RHIRootParamType::CBV, 2, 1, RHIShaderVisibility::Pixel);
	addTable(RHIRootParamType::SRV, 0, 8, RHIShaderVisibility::Pixel);

	RHIRootStaticSampler sampler{};
	sampler.shaderRegister = 0;
	sampler.space = 0;
	sampler.filter = RHIFilterMode::Linear;
	sampler.addressU = RHIAddressMode::Wrap;
	sampler.addressV = RHIAddressMode::Wrap;
	sampler.addressW = RHIAddressMode::Wrap;
	sampler.visibility = RHIShaderVisibility::Pixel;
	layout.staticSamplers.push_back(sampler);

	return layout;
}
