#include "Engine/Renderer/Material/MaterialBindCache.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/RHI/Common/RHIRootSignatureLayout.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHITransientDescriptorAllocator.h"
#include "Engine/RHI/Interface/RHIUploadBuffer.h"

#include <cstring>
#include <optional>

namespace
{
std::optional<size_t> FindConstantLayoutIndex(
	const Material& material,
	uint32_t registerIndex,
	uint32_t space)
{
	for (size_t layoutIndex = 0; layoutIndex < material.constantLayout.size(); ++layoutIndex)
	{
		const ShaderConstantBuffer& layout = material.constantLayout[layoutIndex];
		if (layout.Register == registerIndex && layout.Space == space)
		{
			return layoutIndex;
		}
	}

	return std::nullopt;
}

uint32_t ResolveRootConstantCount(const Material& material, const ShaderRootBindingSlot& slot)
{
	if (slot.RootParameterIndex < material.rootSignatureLayout.parameters.size())
	{
		const RHIRootParameterDesc& parameter =
			material.rootSignatureLayout.parameters[slot.RootParameterIndex];
		if (parameter.kind == RHIRootParameterKind::Constants)
		{
			return parameter.constants.num32BitValues;
		}
	}

	return slot.BindCount;
}
} // namespace

MaterialBindCache::MaterialBindCache(RHIDevice* device)
	: m_device(device)
{
}

bool MaterialBindCache::Bind(
	FrameContext& frameContext,
	RHICommandList* commandList,
	const Material& material,
	const MaterialInstance& instance,
	TextureSystemServices& textureServices) const
{
	if (m_device == nullptr || commandList == nullptr || frameContext.transientDescriptors == nullptr)
	{
		LOG_FATAL(LogCategory::Renderer, "MaterialBindCache::Bind called with invalid dependencies");
		return false;
	}

	commandList->SetTransientDescriptorHeap(frameContext.transientDescriptors);

	size_t textureIndex = 0;
	size_t constantIndex = 0;

	for (const ShaderRootBindingSlot& slot : material.bindingSlots)
	{
		if (slot.IsRootConstants)
		{
			const std::optional<size_t> layoutIndex =
				FindConstantLayoutIndex(material, slot.Register, slot.Space);
			if (!layoutIndex.has_value())
			{
				LOG_ERROR(LogCategory::Renderer,
					"Material bind failed: root constants layout not found");
				return false;
			}

			if (*layoutIndex >= instance.constantBuffers.size())
			{
				LOG_ERROR(LogCategory::Renderer,
					"Material bind failed: root constants data missing from material instance");
				return false;
			}

			const std::vector<std::byte>& constantData = instance.constantBuffers[*layoutIndex];
			if (constantData.empty())
			{
				LOG_ERROR(LogCategory::Renderer,
					"Material bind failed: root constants data is empty");
				return false;
			}

			const uint32_t num32BitValues = ResolveRootConstantCount(material, slot);
			const size_t requiredBytes = static_cast<size_t>(num32BitValues) * sizeof(uint32_t);
			if (constantData.size() < requiredBytes)
			{
				LOG_ERROR(LogCategory::Renderer,
					"Material bind failed: root constants data is too small");
				return false;
			}

			commandList->SetGraphicsRoot32BitConstants(
				slot.RootParameterIndex,
				num32BitValues,
				constantData.data(),
				0);
			continue;
		}

		if (slot.RootType != RHIRootParamType::SRV && slot.RootType != RHIRootParamType::CBV)
		{
			continue;
		}

		const uint32_t descriptorIndex = frameContext.transientDescriptors->Allocate();
		if (descriptorIndex == UINT32_MAX)
		{
			LOG_ERROR(LogCategory::RHI, "Transient descriptor allocation failed during material bind");
			return false;
		}

		const CpuDescHandle cpuHandle = frameContext.transientDescriptors->GetCpuHandle(descriptorIndex);
		const GpuDescHandle gpuHandle = frameContext.transientDescriptors->GetGpuHandle(descriptorIndex);

		if (slot.RootType == RHIRootParamType::SRV)
		{
			if (textureIndex >= instance.boundTextures.size())
			{
				LOG_FATAL(LogCategory::Renderer, "Material instance missing bound texture");
				return false;
			}

			Texture* texture = textureServices.GetTexture(instance.boundTextures[textureIndex]);
			if (texture == nullptr || texture->resource == nullptr)
			{
				LOG_ERROR(LogCategory::Renderer, "Material bound texture is invalid");
				return false;
			}

			m_device->WriteShaderResourceView(texture->resource.get(), cpuHandle);
			++textureIndex;
		}
		else if (slot.RootType == RHIRootParamType::CBV)
		{
			if (constantIndex >= instance.constantBuffers.size() || frameContext.uploadBuffer == nullptr)
			{
				LOG_FATAL(LogCategory::Renderer, "Material instance missing constant buffer data");
				return false;
			}

			const std::vector<std::byte>& constantData = instance.constantBuffers[constantIndex];
			if (constantData.empty())
			{
				LOG_ERROR(LogCategory::Renderer, "Material constant buffer data is empty");
				return false;
			}

			const size_t constantSize = constantData.size();
			RHIUploadAllocation allocation = frameContext.uploadBuffer->Allocate(constantSize, 256);
			if (allocation.cpuAddress == nullptr)
			{
				LOG_ERROR(LogCategory::RHI, "Upload buffer allocation failed during material bind");
				return false;
			}

			std::memcpy(allocation.cpuAddress, constantData.data(), constantSize);
			m_device->WriteConstantBufferView(
				frameContext.uploadBuffer,
				allocation.offset,
				static_cast<uint32_t>(constantSize),
				cpuHandle);
			++constantIndex;
		}

		commandList->SetGraphicsRootDescriptorTable(slot.RootParameterIndex, gpuHandle.ptr);
	}

	return true;
}
