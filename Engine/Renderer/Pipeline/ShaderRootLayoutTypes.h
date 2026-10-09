#pragma once

#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionTypes.h"
#include "Engine/RHI/Common/RHIRootSignatureLayout.h"

struct ShaderRootLayoutBuildOptions
{
	// Small cbuffer bindings (e.g. b0, <= 4 DWORDs) become root constants; excluded from CBV tables.
	bool promoteSmallConstantBuffersToRootConstants = true;
	std::uint32_t maxRootConstantsDwords = 4;

	// One root parameter per (descriptor type, register space, shader visibility).
	// Constant buffers are further split per register so each cbuffer can be bound on its own.
	bool splitDescriptorTables = true;
	bool mergeContiguousRanges = true;
	bool allowUnbounded = true;
	bool allowInputAssembler = true;

	enum class SamplerStrategy : std::uint8_t
	{
		StaticSamplerDefaults,
		Skip,
	};

	SamplerStrategy samplerStrategy = SamplerStrategy::StaticSamplerDefaults;
};

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

struct ShaderRootLayoutData
{
	RHIRootSignatureLayout Layout{};
	std::vector<ShaderRootBindingSlot> Slots{};
};
