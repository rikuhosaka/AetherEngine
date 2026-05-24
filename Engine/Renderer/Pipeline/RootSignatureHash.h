#pragma once

#include "Engine/Core/Hash/HashCombine.h"
#include "Engine/RHI/Common/RHIRootSignatureLayout.h"

inline void HashCombineRootConstants(std::size_t& seed, const RHIRootConstants& constants)
{
	HashCombine(seed, static_cast<std::size_t>(constants.num32BitValues));
	HashCombine(seed, static_cast<std::size_t>(constants.baseRegister));
	HashCombine(seed, static_cast<std::size_t>(constants.space));
}

inline void HashCombineRootDescriptorRange(std::size_t& seed, const RHIRootDescriptorRange& range)
{
	HashCombine(seed, static_cast<std::size_t>(range.type));
	HashCombine(seed, static_cast<std::size_t>(range.count));
	HashCombine(seed, static_cast<std::size_t>(range.baseRegister));
	HashCombine(seed, static_cast<std::size_t>(range.space));
	HashCombine(seed, static_cast<std::size_t>(range.offset));
	HashCombine(seed, static_cast<std::size_t>(range.unbounded));
}

inline void HashCombineRootDescriptor(std::size_t& seed, const RHIRootDescriptor& descriptor)
{
	HashCombine(seed, static_cast<std::size_t>(descriptor.type));
	HashCombine(seed, static_cast<std::size_t>(descriptor.shaderRegister));
	HashCombine(seed, static_cast<std::size_t>(descriptor.space));
}

inline void HashCombineRootParameter(std::size_t& seed, const RHIRootParameterDesc& parameter)
{
	HashCombine(seed, static_cast<std::size_t>(parameter.kind));
	HashCombine(seed, static_cast<std::size_t>(parameter.visibility));

	switch (parameter.kind)
	{
	case RHIRootParameterKind::Constants:
		HashCombineRootConstants(seed, parameter.constants);
		break;

	case RHIRootParameterKind::DescriptorTable:
		HashCombine(seed, parameter.ranges.size());
		for (const RHIRootDescriptorRange& range : parameter.ranges)
		{
			HashCombineRootDescriptorRange(seed, range);
		}
		break;

	case RHIRootParameterKind::RootCBV:
	case RHIRootParameterKind::RootSRV:
	case RHIRootParameterKind::RootUAV:
		HashCombineRootDescriptor(seed, parameter.descriptor);
		break;
	}
}

inline void HashCombineStaticSampler(std::size_t& seed, const RHIRootStaticSampler& sampler)
{
	HashCombine(seed, static_cast<std::size_t>(sampler.shaderRegister));
	HashCombine(seed, static_cast<std::size_t>(sampler.space));
	HashCombine(seed, static_cast<std::size_t>(sampler.filter));
	HashCombine(seed, static_cast<std::size_t>(sampler.addressU));
	HashCombine(seed, static_cast<std::size_t>(sampler.addressV));
	HashCombine(seed, static_cast<std::size_t>(sampler.addressW));
	HashCombine(seed, static_cast<std::size_t>(sampler.visibility));
}

[[nodiscard]] inline std::uint64_t HashRootSignatureLayout(const RHIRootSignatureLayout& layout)
{
	std::size_t hash = 0;

	HashCombine(hash, static_cast<std::size_t>(layout.flags));
	HashCombine(hash, layout.parameters.size());
	HashCombine(hash, layout.staticSamplers.size());

	for (const RHIRootParameterDesc& parameter : layout.parameters)
	{
		HashCombineRootParameter(hash, parameter);
	}

	for (const RHIRootStaticSampler& sampler : layout.staticSamplers)
	{
		HashCombineStaticSampler(hash, sampler);
	}

	return static_cast<std::uint64_t>(hash);
}
