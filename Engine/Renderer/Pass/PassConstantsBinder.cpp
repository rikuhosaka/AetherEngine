#include "Engine/Renderer/Pass/FrameConstantsBinder.h"

#include "Engine/Renderer/Pass/ObjectConstantsBinder.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Renderer/Scene/RenderConstantsLayout.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHITransientDescriptorAllocator.h"
#include "Engine/RHI/Interface/RHIUploadBuffer.h"

#include <cstring>

namespace
{
[[nodiscard]] const ShaderRootBindingSlot* FindCbvBindingSlot(
	const Material& material,
	uint32_t registerIndex,
	uint32_t space)
{
	for (const ShaderRootBindingSlot& slot : material.bindingSlots)
	{
		if (slot.IsRootConstants)
		{
			continue;
		}

		if (slot.RootType != RHIRootParamType::CBV)
		{
			continue;
		}

		if (slot.Register == registerIndex && slot.Space == space)
		{
			return &slot;
		}
	}

	return nullptr;
}

[[nodiscard]] bool BindConstantBufferAtRegister(
	RHIDevice* device,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const Material& material,
	uint32_t registerIndex,
	uint32_t space,
	const void* data,
	size_t dataSize)
{
	if (device == nullptr || commandList == nullptr || data == nullptr || dataSize == 0)
	{
		LOG_FATAL(LogCategory::Renderer, "Pass constant bind called with invalid arguments");
		return false;
	}

	if (frameContext.transientDescriptors == nullptr || frameContext.uploadBuffer == nullptr)
	{
		LOG_FATAL(LogCategory::Renderer, "Pass constant bind requires transient descriptors and upload buffer");
		return false;
	}

	const ShaderRootBindingSlot* bindingSlot = FindCbvBindingSlot(material, registerIndex, space);
	if (bindingSlot == nullptr)
	{
		return false;
	}

	const uint32_t descriptorIndex = frameContext.transientDescriptors->Allocate();
	if (descriptorIndex == UINT32_MAX)
	{
		LOG_ERROR(LogCategory::RHI, "Transient descriptor allocation failed during pass constant bind");
		return false;
	}

	RHIUploadAllocation allocation = frameContext.uploadBuffer->Allocate(dataSize, 256);
	if (allocation.cpuAddress == nullptr)
	{
		LOG_ERROR(LogCategory::RHI, "Upload buffer allocation failed during pass constant bind");
		return false;
	}

	std::memcpy(allocation.cpuAddress, data, dataSize);

	const CpuDescHandle cpuHandle = frameContext.transientDescriptors->GetCpuHandle(descriptorIndex);
	const GpuDescHandle gpuHandle = frameContext.transientDescriptors->GetGpuHandle(descriptorIndex);

	device->WriteConstantBufferView(
		frameContext.uploadBuffer,
		allocation.offset,
		static_cast<uint32_t>(dataSize),
		cpuHandle);

	commandList->SetGraphicsRootDescriptorTable(bindingSlot->RootParameterIndex, gpuHandle.ptr);
	return true;
}
} // namespace

FrameConstantsBinder::FrameConstantsBinder(RHIDevice* device)
	: m_device(device)
{
}

bool FrameConstantsBinder::Bind(
	FrameContext& frameContext,
	RHICommandList* commandList,
	const Material& material,
	const FrameConstants& constants) const
{
	if (commandList == nullptr)
	{
		LOG_FATAL(LogCategory::Renderer, "FrameConstantsBinder::Bind called with null command list");
		return false;
	}

	commandList->SetTransientDescriptorHeap(frameContext.transientDescriptors);

	return BindConstantBufferAtRegister(
		m_device,
		frameContext,
		commandList,
		material,
		RenderRegisters::FrameConstants,
		0,
		&constants,
		sizeof(FrameConstants));
}

ObjectConstantsBinder::ObjectConstantsBinder(RHIDevice* device)
	: m_device(device)
{
}

bool ObjectConstantsBinder::Bind(
	FrameContext& frameContext,
	RHICommandList* commandList,
	const Material& material,
	const ObjectConstants& constants) const
{
	if (commandList == nullptr)
	{
		LOG_FATAL(LogCategory::Renderer, "ObjectConstantsBinder::Bind called with null command list");
		return false;
	}

	commandList->SetTransientDescriptorHeap(frameContext.transientDescriptors);

	return BindConstantBufferAtRegister(
		m_device,
		frameContext,
		commandList,
		material,
		RenderRegisters::ObjectConstants,
		0,
		&constants,
		sizeof(ObjectConstants));
}
