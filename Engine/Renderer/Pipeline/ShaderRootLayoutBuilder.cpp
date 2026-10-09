#include "Engine/Renderer/Pipeline/ShaderRootLayoutBuilder.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"

#include <bit>
#include <format>
#include <map>
#include <optional>
#include <tuple>
#include <unordered_map>

namespace
{
	struct MergedBinding
	{
		std::string Name{};
		ShaderResourceType ShaderType = ShaderResourceType::Unknown;
		RHIRootParamType RootType = RHIRootParamType::CBV;
		ShaderResourceAccess Access = ShaderResourceAccess::ReadOnly;

		std::uint32_t Register = 0;
		std::uint32_t Space = 0;
		std::uint32_t BindCount = 1;
		bool Unbounded = false;

		ShaderStageFlags StageVisibility = ShaderStageFlags::None;
		bool PromotedToRootConstants = false;
	};

	struct RootConstantsCandidate
	{
		std::uint32_t Register = 0;
		std::uint32_t Space = 0;
		std::uint32_t Num32BitValues = 0;
		RHIShaderVisibility Visibility = RHIShaderVisibility::Vertex;
		std::string Name{};
	};

	[[nodiscard]] std::uint64_t BindingKey(
		RHIRootParamType rootType,
		std::uint32_t space,
		std::uint32_t registerIndex)
	{
		return (static_cast<std::uint64_t>(rootType) << 48) |
			(static_cast<std::uint64_t>(space) << 32) |
			static_cast<std::uint64_t>(registerIndex);
	}

	[[nodiscard]] bool IsSamplerResource(ShaderResourceType type)
	{
		return type == ShaderResourceType::Sampler;
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

	[[nodiscard]] RHIShaderVisibility StageToVisibility(ShaderStage stage)
	{
		switch (stage)
		{
		case ShaderStage::Vertex:
			return RHIShaderVisibility::Vertex;
		case ShaderStage::Pixel:
			return RHIShaderVisibility::Pixel;
		case ShaderStage::Compute:
			return RHIShaderVisibility::Compute;
		case ShaderStage::Geometry:
			return RHIShaderVisibility::Geometry;
		case ShaderStage::Hull:
			return RHIShaderVisibility::Hull;
		case ShaderStage::Domain:
			return RHIShaderVisibility::Domain;
		default:
			return RHIShaderVisibility::All;
		}
	}

	[[nodiscard]] RHIShaderVisibility ToRHIShaderVisibility(
		ShaderStageFlags visibility,
		ShaderStage fallbackStage)
	{
		const std::uint32_t graphicsMask = static_cast<std::uint32_t>(ShaderStageFlags::AllGraphics);
		const std::uint32_t visibleGraphics =
			static_cast<std::uint32_t>(visibility) & graphicsMask;

		if (visibleGraphics == 0)
		{
			return StageToVisibility(fallbackStage);
		}

		const auto hasStage = [&](ShaderStageFlags flag) -> bool
		{
			return (visibility & flag) != ShaderStageFlags::None;
		};

		const int stageCount = static_cast<int>(std::popcount(visibleGraphics));
		if (stageCount == 1)
		{
			if (hasStage(ShaderStageFlags::Vertex))
			{
				return RHIShaderVisibility::Vertex;
			}
			if (hasStage(ShaderStageFlags::Pixel))
			{
				return RHIShaderVisibility::Pixel;
			}
			if (hasStage(ShaderStageFlags::Geometry))
			{
				return RHIShaderVisibility::Geometry;
			}
			if (hasStage(ShaderStageFlags::Hull))
			{
				return RHIShaderVisibility::Hull;
			}
			if (hasStage(ShaderStageFlags::Domain))
			{
				return RHIShaderVisibility::Domain;
			}
		}

		if (hasStage(ShaderStageFlags::Vertex) && hasStage(ShaderStageFlags::Pixel))
		{
			return RHIShaderVisibility::All;
		}

		return RHIShaderVisibility::All;
	}

	[[nodiscard]] int VisibilitySortOrder(RHIShaderVisibility visibility)
	{
		switch (visibility)
		{
		case RHIShaderVisibility::Vertex:
			return 0;
		case RHIShaderVisibility::Hull:
			return 1;
		case RHIShaderVisibility::Domain:
			return 2;
		case RHIShaderVisibility::Geometry:
			return 3;
		case RHIShaderVisibility::Pixel:
			return 4;
		case RHIShaderVisibility::All:
			return 5;
		case RHIShaderVisibility::Compute:
			return 6;
		default:
			return 7;
		}
	}

	[[nodiscard]] int RootTypeSortOrder(RHIRootParamType type)
	{
		switch (type)
		{
		case RHIRootParamType::CBV:
			return 0;
		case RHIRootParamType::SRV:
			return 1;
		case RHIRootParamType::UAV:
			return 2;
		default:
			return 3;
		}
	}

	[[nodiscard]] const ShaderConstantBuffer* FindConstantBuffer(
		const std::vector<ShaderReflectionData>& stages,
		std::uint32_t registerIndex,
		std::uint32_t space)
	{
		for (const ShaderReflectionData& stage : stages)
		{
			for (const ShaderConstantBuffer& constantBuffer : stage.ConstantBuffers)
			{
				if (constantBuffer.Register == registerIndex && constantBuffer.Space == space)
				{
					return &constantBuffer;
				}
			}
		}
		return nullptr;
	}

	[[nodiscard]] std::optional<RootConstantsCandidate> SelectRootConstantsCandidate(
		const std::vector<MergedBinding>& bindings,
		const std::vector<ShaderReflectionData>& stages,
		const ShaderRootLayoutBuildOptions& options)
	{
		if (!options.promoteSmallConstantBuffersToRootConstants ||
			options.maxRootConstantsDwords == 0)
		{
			return std::nullopt;
		}

		std::optional<RootConstantsCandidate> best{};

		for (const MergedBinding& binding : bindings)
		{
			if (binding.ShaderType != ShaderResourceType::ConstantBuffer ||
				binding.PromotedToRootConstants)
			{
				continue;
			}

			const ShaderConstantBuffer* constantBuffer =
				FindConstantBuffer(stages, binding.Register, binding.Space);
			if (constantBuffer == nullptr)
			{
				continue;
			}

			const std::uint32_t byteSize =
				constantBuffer->AlignedSize > 0
					? constantBuffer->AlignedSize
					: constantBuffer->Size;
			if (byteSize == 0)
			{
				continue;
			}

			const std::uint32_t num32BitValues = (byteSize + 3u) / 4u;
			if (num32BitValues == 0 || num32BitValues > options.maxRootConstantsDwords ||
				num32BitValues > 64u)
			{
				continue;
			}

			RootConstantsCandidate candidate{
				.Register = binding.Register,
				.Space = binding.Space,
				.Num32BitValues = num32BitValues,
				.Visibility = ToRHIShaderVisibility(binding.StageVisibility, ShaderStage::Unknown),
				.Name = binding.Name,
			};

			if (!best.has_value())
			{
				best = candidate;
				continue;
			}

			if (candidate.Space < best->Space ||
				(candidate.Space == best->Space && candidate.Register < best->Register) ||
				(candidate.Space == best->Space && candidate.Register == best->Register &&
					candidate.Num32BitValues < best->Num32BitValues))
			{
				best = candidate;
			}
		}

		return best;
	}

	[[nodiscard]] std::uint32_t EstimateRootSignatureDwords(const RHIRootSignatureLayout& layout)
	{
		std::uint32_t cost = 0;
		for (const RHIRootParameterDesc& parameter : layout.parameters)
		{
			switch (parameter.kind)
			{
			case RHIRootParameterKind::Constants:
				cost += parameter.constants.num32BitValues;
				break;
			case RHIRootParameterKind::DescriptorTable:
				cost += 1;
				break;
			case RHIRootParameterKind::RootCBV:
			case RHIRootParameterKind::RootSRV:
			case RHIRootParameterKind::RootUAV:
				cost += 2;
				break;
			}
		}
		return cost;
	}

	[[nodiscard]] Result<ShaderRootLayoutData> MakeLayoutFail(std::string error)
	{
		return FailRuntime<ShaderRootLayoutData>(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			std::move(error));
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

	[[nodiscard]] std::vector<MergedBinding> MergeStageBindings(
		const std::vector<ShaderReflectionData>& stages,
		std::string& error)
	{
		std::unordered_map<std::uint64_t, std::size_t> bindingIndexByKey{};
		std::vector<MergedBinding> merged{};

		for (const ShaderReflectionData& stage : stages)
		{
			const ShaderStageFlags stageFlag = StageToFlags(stage.Stage);

			for (const ShaderResourceBinding& binding : stage.Bindings)
			{
				if (IsUnknownResource(binding.Type))
				{
					error = std::format(
						"Unknown shader resource binding '{}'.",
						binding.Name);
					return {};
				}

				const RHIRootParamType rootType = ToRootParamType(binding.Type);
				const std::uint64_t key = BindingKey(rootType, binding.Space, binding.Register);

				ShaderStageFlags bindingVisibility = binding.Visibility;
				if (bindingVisibility == ShaderStageFlags::None)
				{
					bindingVisibility = stageFlag;
				}

				const auto found = bindingIndexByKey.find(key);
				if (found == bindingIndexByKey.end())
				{
					MergedBinding mergedBinding{};
					mergedBinding.Name = binding.Name;
					mergedBinding.ShaderType = binding.Type;
					mergedBinding.RootType = rootType;
					mergedBinding.Access = binding.Access;
					mergedBinding.Register = binding.Register;
					mergedBinding.Space = binding.Space;
					mergedBinding.BindCount = binding.BindCount == 0 ? 1u : binding.BindCount;
					mergedBinding.Unbounded = binding.IsBindless;
					mergedBinding.StageVisibility = bindingVisibility;
					bindingIndexByKey.emplace(key, merged.size());
					merged.push_back(std::move(mergedBinding));
					continue;
				}

				MergedBinding& existing = merged[found->second];
				if (existing.ShaderType != binding.Type)
				{
					error = std::format(
						"Conflicting resource types at register {} space {} ('{}' vs '{}').",
						binding.Register,
						binding.Space,
						existing.Name,
						binding.Name);
					return {};
				}

				existing.StageVisibility |= bindingVisibility;
				const std::uint32_t bindingCount = binding.BindCount == 0 ? 1u : binding.BindCount;
				if (bindingCount > existing.BindCount)
				{
					existing.BindCount = bindingCount;
				}
				existing.Unbounded = existing.Unbounded || binding.IsBindless;
				if (existing.Name.empty())
				{
					existing.Name = binding.Name;
				}
			}
		}

		return merged;
	}

	// The last field isolates each constant buffer register into its own descriptor table.
	// SRV and UAV ranges stay grouped, so that field is zero for them.
	using TableBucketKey = std::tuple<RHIRootParamType, std::uint32_t, RHIShaderVisibility, std::uint32_t>;

	[[nodiscard]] Result<ShaderRootLayoutData> BuildLayout(
		const std::vector<ShaderReflectionData>& stages,
		ShaderRootLayoutBuildOptions options)
	{
		if (stages.empty())
		{
			return MakeLayoutFail("No shader reflection stages were provided.");
		}

		std::string mergeError{};
		std::vector<MergedBinding> bindings = MergeStageBindings(stages, mergeError);
		if (!mergeError.empty())
		{
			return MakeLayoutFail(std::move(mergeError));
		}

		ShaderRootLayoutData data{};
		data.Layout.flags = RHIRootSignatureFlags::None;

		bool hasInputElements = false;
		for (const ShaderReflectionData& stage : stages)
		{
			if (!stage.InputElements.empty())
			{
				hasInputElements = true;
				break;
			}
		}
		if (hasInputElements && options.allowInputAssembler)
		{
			data.Layout.flags =
				data.Layout.flags | RHIRootSignatureFlags::AllowInputAssembler;
		}

		std::optional<RootConstantsCandidate> rootConstants =
			SelectRootConstantsCandidate(bindings, stages, options);
		if (rootConstants.has_value())
		{
			for (MergedBinding& binding : bindings)
			{
				if (binding.ShaderType == ShaderResourceType::ConstantBuffer &&
					binding.Register == rootConstants->Register &&
					binding.Space == rootConstants->Space)
				{
					binding.PromotedToRootConstants = true;
					break;
				}
			}

			RHIRootParameterDesc constantsParam{};
			constantsParam.kind = RHIRootParameterKind::Constants;
			constantsParam.visibility = rootConstants->Visibility;
			constantsParam.constants.num32BitValues = rootConstants->Num32BitValues;
			constantsParam.constants.baseRegister = rootConstants->Register;
			constantsParam.constants.space = rootConstants->Space;
			data.Layout.parameters.push_back(constantsParam);

			ShaderRootBindingSlot slot{};
			slot.Name = rootConstants->Name;
			slot.ShaderType = ShaderResourceType::ConstantBuffer;
			slot.RootType = RHIRootParamType::CBV;
			slot.Register = rootConstants->Register;
			slot.Space = rootConstants->Space;
			slot.BindCount = rootConstants->Num32BitValues;
			slot.RootParameterIndex = 0;
			slot.Visibility = rootConstants->Visibility;
			slot.IsRootConstants = true;
			data.Slots.push_back(std::move(slot));
		}

		std::map<TableBucketKey, std::vector<MergedBinding*>> tableBuckets{};

		for (MergedBinding& binding : bindings)
		{
			if (binding.PromotedToRootConstants)
			{
				continue;
			}

			if (IsSamplerResource(binding.ShaderType))
			{
				if (options.samplerStrategy == ShaderRootLayoutBuildOptions::SamplerStrategy::Skip)
				{
					continue;
				}

				RHIRootStaticSampler staticSampler{};
				staticSampler.shaderRegister = binding.Register;
				staticSampler.space = binding.Space;
				staticSampler.visibility =
					ToRHIShaderVisibility(binding.StageVisibility, ShaderStage::Unknown);
				data.Layout.staticSamplers.push_back(staticSampler);

				ShaderRootBindingSlot slot{};
				slot.Name = binding.Name;
				slot.ShaderType = binding.ShaderType;
				slot.RootType = RHIRootParamType::StaticSampler;
				slot.Register = binding.Register;
				slot.Space = binding.Space;
				slot.BindCount = 1;
				slot.Visibility = staticSampler.visibility;
				data.Slots.push_back(std::move(slot));
				continue;
			}

			if (!options.splitDescriptorTables)
			{
				return MakeLayoutFail("Phase 1 requires splitDescriptorTables = true.");
			}

			const RHIShaderVisibility visibility =
				ToRHIShaderVisibility(binding.StageVisibility, ShaderStage::Unknown);
			const std::uint32_t constantBufferRegister =
				binding.RootType == RHIRootParamType::CBV ? binding.Register : 0u;
			const TableBucketKey bucketKey{
				binding.RootType,
				binding.Space,
				visibility,
				constantBufferRegister};
			tableBuckets[bucketKey].push_back(&binding);
		}

		std::vector<TableBucketKey> bucketOrder{};
		bucketOrder.reserve(tableBuckets.size());
		for (const auto& [key, _] : tableBuckets)
		{
			bucketOrder.push_back(key);
		}

		std::sort(bucketOrder.begin(), bucketOrder.end(),
			[](const TableBucketKey& lhs, const TableBucketKey& rhs)
			{
				const auto& [lhsType, lhsSpace, lhsVisibility, lhsRegister] = lhs;
				const auto& [rhsType, rhsSpace, rhsVisibility, rhsRegister] = rhs;
				if (RootTypeSortOrder(lhsType) != RootTypeSortOrder(rhsType))
				{
					return RootTypeSortOrder(lhsType) < RootTypeSortOrder(rhsType);
				}
				if (VisibilitySortOrder(lhsVisibility) != VisibilitySortOrder(rhsVisibility))
				{
					return VisibilitySortOrder(lhsVisibility) < VisibilitySortOrder(rhsVisibility);
				}
				if (lhsSpace != rhsSpace)
				{
					return lhsSpace < rhsSpace;
				}
				return lhsRegister < rhsRegister;
			});

		std::optional<std::string> buildError;
		for (const TableBucketKey& bucketKey : bucketOrder)
		{
			std::vector<MergedBinding*>& bucketBindings = tableBuckets[bucketKey];
			const auto& [rootType, space, visibility, constantBufferRegister] = bucketKey;
			(void)constantBufferRegister;

			std::sort(bucketBindings.begin(), bucketBindings.end(),
				[](const MergedBinding* lhs, const MergedBinding* rhs)
				{
					return lhs->Register < rhs->Register;
				});

			RHIRootParameterDesc tableParam{};
			tableParam.kind = RHIRootParameterKind::DescriptorTable;
			tableParam.visibility = visibility;

			std::uint32_t tableOffset = 0;
			const std::uint32_t rootParameterIndex =
				static_cast<std::uint32_t>(data.Layout.parameters.size());

			auto appendRange = [&](const MergedBinding& binding, std::uint32_t rangeIndex)
			{
				if (binding.Unbounded && !options.allowUnbounded)
				{
					buildError = std::format(
						"Bindless resource '{}' is not allowed by build options.",
						binding.Name);
					return false;
				}

				RHIRootDescriptorRange range{};
				range.type = rootType;
				range.baseRegister = binding.Register;
				range.space = space;
				range.offset = tableOffset;
				range.unbounded = binding.Unbounded;
				range.count = binding.Unbounded ? 1u : binding.BindCount;
				tableParam.ranges.push_back(range);

				ShaderRootBindingSlot slot{};
				slot.Name = binding.Name;
				slot.ShaderType = binding.ShaderType;
				slot.RootType = rootType;
				slot.Register = binding.Register;
				slot.Space = space;
				slot.BindCount = range.count;
				slot.RootParameterIndex = rootParameterIndex;
				slot.RangeIndex = rangeIndex;
				slot.TableOffset = tableOffset;
				slot.Visibility = visibility;
				data.Slots.push_back(std::move(slot));

				if (!binding.Unbounded)
				{
					tableOffset += binding.BindCount;
				}
				return true;
			};

			if (!options.mergeContiguousRanges || bucketBindings.empty())
			{
				for (std::size_t bindingIndex = 0; bindingIndex < bucketBindings.size(); ++bindingIndex)
				{
					if (!appendRange(*bucketBindings[bindingIndex], static_cast<std::uint32_t>(bindingIndex)))
					{
						return MakeLayoutFail(buildError.value_or("Root layout build failed."));
					}
				}
			}
			else
			{
				std::size_t rangeIndex = 0;
				for (std::size_t bindingIndex = 0; bindingIndex < bucketBindings.size();)
				{
					const MergedBinding& first = *bucketBindings[bindingIndex];
					MergedBinding mergedRange = first;

					std::size_t nextIndex = bindingIndex + 1;
					while (nextIndex < bucketBindings.size() && !mergedRange.Unbounded)
					{
						const MergedBinding& next = *bucketBindings[nextIndex];
						const std::uint32_t endRegister =
							mergedRange.Register + mergedRange.BindCount;
						if (next.Register != endRegister || next.Unbounded)
						{
							break;
						}

						mergedRange.BindCount += next.BindCount;
						if (next.Name.size() > mergedRange.Name.size())
						{
							mergedRange.Name = next.Name;
						}
						++nextIndex;
					}

					if (!appendRange(mergedRange, static_cast<std::uint32_t>(rangeIndex)))
					{
						return MakeLayoutFail(buildError.value_or("Root layout build failed."));
					}

					++rangeIndex;
					bindingIndex = nextIndex;
				}
			}

			if (!tableParam.ranges.empty())
			{
				data.Layout.parameters.push_back(std::move(tableParam));
			}
		}

		const std::uint32_t rootCost = EstimateRootSignatureDwords(data.Layout);
		if (rootCost > 64u)
		{
			return MakeLayoutFail(std::format(
				"Root signature cost {} exceeds D3D12 limit of 64 DWORDs.",
				rootCost));
		}

		return MakeOk(std::move(data));
	}
}

void ShaderRootLayoutBuilder::Clear()
{
	m_stages.clear();
}

void ShaderRootLayoutBuilder::AddStage(const ShaderReflectionData& reflection)
{
	m_stages.push_back(reflection);
}

void ShaderRootLayoutBuilder::SetOptions(ShaderRootLayoutBuildOptions options)
{
	m_options = options;
}

Result<ShaderRootLayoutData> ShaderRootLayoutBuilder::Build() const
{
	return BuildLayout(m_stages, m_options);
}

Result<ShaderRootLayoutData> BuildRootSignatureLayout(
	std::span<const ShaderReflectionData> stages,
	ShaderRootLayoutBuildOptions options)
{
	std::vector<ShaderReflectionData> copied(stages.begin(), stages.end());
	return BuildLayout(copied, options);
}
