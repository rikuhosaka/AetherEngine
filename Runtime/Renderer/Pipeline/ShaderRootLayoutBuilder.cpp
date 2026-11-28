#include "Runtime/Renderer/Pipeline/ShaderRootLayoutBuilder.h"

#include "Runtime/Core/Log/Result.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <unordered_map>

namespace
{
	struct MergedBinding
	{
		std::string Name{};
		ShaderResourceType ShaderType = ShaderResourceType::Unknown;
		RHIRootParamType RootType = RHIRootParamType::CBV;
		std::uint32_t Register = 0;
		std::uint32_t Space = 0;
		std::uint32_t BindCount = 1;
		bool Unbounded = false;
		ShaderStageFlags StageVisibility = ShaderStageFlags::None;
	};

	struct ParameterMatch
	{
		std::uint32_t RootParameterIndex = 0;
		std::uint32_t RangeIndex = 0;
		std::uint32_t TableOffset = 0;
		RHIShaderVisibility Visibility = RHIShaderVisibility::All;
		bool IsRootConstants = false;
	};

	[[nodiscard]] Result<std::vector<ShaderRootBindingSlot>> FailMapping(std::string error)
	{
		return FailRuntime<std::vector<ShaderRootBindingSlot>>(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			std::move(error));
	}

	[[nodiscard]] bool HasFlag(RHIRootSignatureFlags flags, RHIRootSignatureFlags flag)
	{
		return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(flag)) != 0;
	}

	[[nodiscard]] bool IsUnknownResource(ShaderResourceType type)
	{
		return type == ShaderResourceType::Unknown;
	}

	[[nodiscard]] RHIRootParamType ToRootParamType(ShaderResourceType type)
	{
		switch (type)
		{
		case ShaderResourceType::ConstantBuffer:
			return RHIRootParamType::CBV;

		case ShaderResourceType::RWTexture1D:
		case ShaderResourceType::RWTexture1DArray:
		case ShaderResourceType::RWTexture2D:
		case ShaderResourceType::RWTexture2DArray:
		case ShaderResourceType::RWTexture3D:
		case ShaderResourceType::RWStructuredBuffer:
		case ShaderResourceType::RWByteAddressBuffer:
			return RHIRootParamType::UAV;

		case ShaderResourceType::Sampler:
			return RHIRootParamType::StaticSampler;

		default:
			return RHIRootParamType::SRV;
		}
	}

	[[nodiscard]] ShaderStageFlags StageToFlags(ShaderStage stage)
	{
		switch (stage)
		{
		case ShaderStage::Vertex:
			return ShaderStageFlags::Vertex;
		case ShaderStage::Pixel:
			return ShaderStageFlags::Pixel;
		case ShaderStage::Compute:
			return ShaderStageFlags::Compute;
		case ShaderStage::Geometry:
			return ShaderStageFlags::Geometry;
		case ShaderStage::Hull:
			return ShaderStageFlags::Hull;
		case ShaderStage::Domain:
			return ShaderStageFlags::Domain;
		default:
			return ShaderStageFlags::None;
		}
	}

	[[nodiscard]] std::uint64_t BindingKey(
		RHIRootParamType rootType,
		std::uint32_t space,
		std::uint32_t registerIndex)
	{
		return (static_cast<std::uint64_t>(rootType) << 48) |
			(static_cast<std::uint64_t>(space) << 32) |
			static_cast<std::uint64_t>(registerIndex);
	}

	[[nodiscard]] bool VisibilityCovers(RHIShaderVisibility visibility, ShaderStageFlags stages)
	{
		const auto covers = [&](ShaderStageFlags stage, RHIShaderVisibility required) -> bool
		{
			if ((stages & stage) == ShaderStageFlags::None)
			{
				return true;
			}

			return visibility == RHIShaderVisibility::All || visibility == required;
		};

		return covers(ShaderStageFlags::Vertex, RHIShaderVisibility::Vertex) &&
			covers(ShaderStageFlags::Pixel, RHIShaderVisibility::Pixel) &&
			covers(ShaderStageFlags::Compute, RHIShaderVisibility::Compute) &&
			covers(ShaderStageFlags::Geometry, RHIShaderVisibility::Geometry) &&
			covers(ShaderStageFlags::Hull, RHIShaderVisibility::Hull) &&
			covers(ShaderStageFlags::Domain, RHIShaderVisibility::Domain);
	}

	[[nodiscard]] bool StageAccessDenied(RHIRootSignatureFlags flags, ShaderStageFlags stages)
	{
		const auto denied = [&](ShaderStageFlags stage, RHIRootSignatureFlags deny) -> bool
		{
			return (stages & stage) != ShaderStageFlags::None && HasFlag(flags, deny);
		};

		return denied(ShaderStageFlags::Vertex, RHIRootSignatureFlags::DenyVertexShaderAccess) ||
			denied(ShaderStageFlags::Pixel, RHIRootSignatureFlags::DenyPixelShaderAccess) ||
			denied(ShaderStageFlags::Compute, RHIRootSignatureFlags::DenyComputeShaderAccess);
	}

	[[nodiscard]] bool RangeCovers(
		const RHIRootDescriptorRange& range,
		RHIRootParamType type,
		std::uint32_t shaderRegister,
		std::uint32_t space)
	{
		if (range.unbounded || range.type != type || range.space != space || range.count == 0)
		{
			return false;
		}

		const std::uint64_t end = static_cast<std::uint64_t>(range.baseRegister) + range.count;
		return shaderRegister >= range.baseRegister &&
			static_cast<std::uint64_t>(shaderRegister) + 1u <= end;
	}

	void CollectParameterMatches(
		const RHIRootSignatureLayout& layout,
		const MergedBinding& binding,
		std::vector<ParameterMatch>& matches)
	{
		if (binding.RootType == RHIRootParamType::StaticSampler)
		{
			return;
		}

		for (std::uint32_t parameterIndex = 0;
			parameterIndex < layout.parameters.size();
			++parameterIndex)
		{
			const RHIRootParameterDesc& parameter = layout.parameters[parameterIndex];
			if (parameter.kind == RHIRootParameterKind::Constants &&
				binding.RootType == RHIRootParamType::CBV &&
				parameter.constants.space == binding.Space &&
				parameter.constants.baseRegister == binding.Register)
			{
				ParameterMatch match{};
				match.RootParameterIndex = parameterIndex;
				match.Visibility = parameter.visibility;
				match.IsRootConstants = true;
				matches.push_back(match);
				continue;
			}

			if (parameter.kind == RHIRootParameterKind::RootCBV ||
				parameter.kind == RHIRootParameterKind::RootSRV ||
				parameter.kind == RHIRootParameterKind::RootUAV)
			{
				if (parameter.descriptor.type == binding.RootType &&
					parameter.descriptor.space == binding.Space &&
					parameter.descriptor.shaderRegister == binding.Register)
				{
					ParameterMatch match{};
					match.RootParameterIndex = parameterIndex;
					match.Visibility = parameter.visibility;
					matches.push_back(match);
				}
				continue;
			}

			if (parameter.kind != RHIRootParameterKind::DescriptorTable)
			{
				continue;
			}

			for (std::uint32_t rangeIndex = 0; rangeIndex < parameter.ranges.size(); ++rangeIndex)
			{
				const RHIRootDescriptorRange& range = parameter.ranges[rangeIndex];
				if (!RangeCovers(range, binding.RootType, binding.Register, binding.Space))
				{
					continue;
				}

				ParameterMatch match{};
				match.RootParameterIndex = parameterIndex;
				match.RangeIndex = rangeIndex;
				match.TableOffset = range.offset + (binding.Register - range.baseRegister);
				match.Visibility = parameter.visibility;
				matches.push_back(match);
			}
		}
	}

	[[nodiscard]] std::string MergeStages(
		std::span<const ShaderReflectionData> stages,
		std::vector<MergedBinding>& merged)
	{
		std::unordered_map<std::uint64_t, std::size_t> bindingIndexByKey{};

		for (const ShaderReflectionData& stage : stages)
		{
			const ShaderStageFlags stageFlag = StageToFlags(stage.Stage);
			for (const ShaderResourceBinding& binding : stage.Bindings)
			{
				if (IsUnknownResource(binding.Type))
				{
					return std::format("Unknown shader resource binding '{}'.", binding.Name);
				}

				if (binding.Space != 0)
				{
					return std::format(
						"Shader resource '{}' uses register space {}, which is not supported.",
						binding.Name,
						binding.Space);
				}

				if (binding.IsBindless || binding.BindCount == UINT32_MAX)
				{
					return std::format("Bindless shader resource '{}' is not supported.", binding.Name);
				}

				if (binding.BindCount != 1)
				{
					return std::format(
						"Shader resource '{}' has bind count {}, which is not supported.",
						binding.Name,
						binding.BindCount);
				}

				const RHIRootParamType rootType = ToRootParamType(binding.Type);
				ShaderStageFlags bindingVisibility = binding.Visibility;
				if (bindingVisibility == ShaderStageFlags::None)
				{
					bindingVisibility = stageFlag;
				}

				const std::uint64_t key = BindingKey(rootType, binding.Space, binding.Register);
				const auto found = bindingIndexByKey.find(key);
				if (found == bindingIndexByKey.end())
				{
					MergedBinding mergedBinding{};
					mergedBinding.Name = binding.Name;
					mergedBinding.ShaderType = binding.Type;
					mergedBinding.RootType = rootType;
					mergedBinding.Register = binding.Register;
					mergedBinding.Space = binding.Space;
					mergedBinding.BindCount = 1;
					mergedBinding.Unbounded = binding.IsBindless;
					mergedBinding.StageVisibility = bindingVisibility;
					bindingIndexByKey.emplace(key, merged.size());
					merged.push_back(std::move(mergedBinding));
					continue;
				}

				MergedBinding& existing = merged[found->second];
				if (existing.ShaderType != binding.Type)
				{
					return std::format(
						"Conflicting resource types at register {} space {} ('{}' vs '{}').",
						binding.Register,
						binding.Space,
						existing.Name,
						binding.Name);
				}

				existing.StageVisibility |= bindingVisibility;
				if (existing.Name.empty())
				{
					existing.Name = binding.Name;
				}
			}
		}

		return {};
	}
}

Result<std::vector<ShaderRootBindingSlot>> MapRootSignatureBindings(
	const RHIRootSignatureLayout& layout,
	std::span<const ShaderReflectionData> stages)
{
	if (stages.empty())
	{
		return FailMapping("No shader reflection stages were provided.");
	}

	bool hasInputElements = false;
	for (const ShaderReflectionData& stage : stages)
	{
		if (!stage.InputElements.empty())
		{
			hasInputElements = true;
			break;
		}
	}

	if (hasInputElements && !HasFlag(layout.flags, RHIRootSignatureFlags::AllowInputAssembler))
	{
		return FailMapping("Shader input signature requires AllowInputAssembler.");
	}

	std::vector<MergedBinding> merged{};
	std::string mergeError = MergeStages(stages, merged);
	if (!mergeError.empty())
	{
		return FailMapping(std::move(mergeError));
	}

	std::vector<ShaderRootBindingSlot> slots{};
	slots.reserve(merged.size());

	for (const MergedBinding& binding : merged)
	{
		if (StageAccessDenied(layout.flags, binding.StageVisibility))
		{
			return FailMapping(std::format(
				"Shader resource '{}' is used by a stage denied by the root signature.",
				binding.Name));
		}

		if (binding.RootType == RHIRootParamType::StaticSampler)
		{
			const RHIRootStaticSampler* sampler = nullptr;
			for (const RHIRootStaticSampler& candidate : layout.staticSamplers)
			{
				if (candidate.space != binding.Space || candidate.shaderRegister != binding.Register)
				{
					continue;
				}

				if (sampler != nullptr)
				{
					return FailMapping(std::format(
						"Static sampler for '{}' matches more than one root signature sampler.",
						binding.Name));
				}

				sampler = &candidate;
			}

			if (sampler == nullptr)
			{
				return FailMapping(std::format(
					"Shader sampler '{}' has no matching static sampler.",
					binding.Name));
			}

			if (!VisibilityCovers(sampler->visibility, binding.StageVisibility))
			{
				return FailMapping(std::format(
					"Static sampler visibility does not cover shader sampler '{}'.",
					binding.Name));
			}

			ShaderRootBindingSlot slot{};
			slot.Name = binding.Name;
			slot.ShaderType = binding.ShaderType;
			slot.RootType = RHIRootParamType::StaticSampler;
			slot.Register = binding.Register;
			slot.Space = binding.Space;
			slot.BindCount = 1;
			slot.Visibility = sampler->visibility;
			slots.push_back(std::move(slot));
			continue;
		}

		std::vector<ParameterMatch> matches{};
		CollectParameterMatches(layout, binding, matches);
		if (matches.empty())
		{
			return FailMapping(std::format(
				"Shader resource '{}' is not declared in the root signature.",
				binding.Name));
		}

		if (matches.size() != 1)
		{
			return FailMapping(std::format(
				"Shader resource '{}' matches more than one root signature parameter.",
				binding.Name));
		}

		const ParameterMatch& match = matches.front();
		if (!VisibilityCovers(match.Visibility, binding.StageVisibility))
		{
			return FailMapping(std::format(
				"Root parameter visibility does not cover shader resource '{}'.",
				binding.Name));
		}

		ShaderRootBindingSlot slot{};
		slot.Name = binding.Name;
		slot.ShaderType = binding.ShaderType;
		slot.RootType = binding.RootType;
		slot.Register = binding.Register;
		slot.Space = binding.Space;
		slot.BindCount = 1;
		slot.RootParameterIndex = match.RootParameterIndex;
		slot.RangeIndex = match.RangeIndex;
		slot.TableOffset = match.TableOffset;
		slot.Visibility = match.Visibility;
		slot.IsRootConstants = match.IsRootConstants;
		slots.push_back(std::move(slot));
	}

	std::sort(slots.begin(), slots.end(),
		[](const ShaderRootBindingSlot& lhs, const ShaderRootBindingSlot& rhs)
		{
			const auto rank = [](const ShaderRootBindingSlot& slot) -> std::uint32_t
			{
				return slot.RootType == RHIRootParamType::StaticSampler
					? UINT32_MAX
					: slot.RootParameterIndex;
			};

			const std::uint32_t lhsRank = rank(lhs);
			const std::uint32_t rhsRank = rank(rhs);
			if (lhsRank != rhsRank)
			{
				return lhsRank < rhsRank;
			}

			return lhs.Register < rhs.Register;
		});

	return MakeOk(std::move(slots));
}
