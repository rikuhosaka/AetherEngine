#include "Engine/RHI/DX12/Descriptor/DX12TransientDescriptorAllocator.h"

#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Descriptor/AllocatorImpl.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"

namespace
{
	constexpr uint32_t kInvalidDescriptorIndex = 0xffffffffu;
}

bool DX12TransientDescriptorAllocator::IsValid() const
{
	return m_impl != nullptr && m_impl->heap != nullptr && m_totalCount > 0;
}

Result<std::unique_ptr<DX12TransientDescriptorAllocator>> DX12TransientDescriptorAllocator::Create(
	uint32_t numDescriptors,
	const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12TransientDescriptorAllocator>(
			new DX12TransientDescriptorAllocator(numDescriptors, dxDevice)),
		"Failed to create transient descriptor allocator");
}

DX12TransientDescriptorAllocator::DX12TransientDescriptorAllocator(uint32_t numDescriptors, const DX12Device* dxDevice)
	: m_impl(std::make_unique<HeapImpl>())
	, m_totalCount(numDescriptors)
	, m_currentOffset(0)
{
	if (m_totalCount == 0)
	{
		return;
	}

	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = m_totalCount;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	const HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_impl->heap));
	if (FAILED(hr))
	{
		m_totalCount = 0;
		return;
	}

	m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_cpuStart = m_impl->heap->GetCPUDescriptorHandleForHeapStart().ptr;
	m_gpuStart = m_impl->heap->GetGPUDescriptorHandleForHeapStart().ptr;
}

DX12TransientDescriptorAllocator::~DX12TransientDescriptorAllocator() = default;

void DX12TransientDescriptorAllocator::Reset()
{
	m_currentOffset = 0;
}

uint32_t DX12TransientDescriptorAllocator::Allocate()
{
	if (!m_impl || !m_impl->heap || m_totalCount == 0)
	{
		return kInvalidDescriptorIndex;
	}
	if (m_currentOffset >= m_totalCount)
	{
		LOG_ERROR(LogCategory::RHI, "Transient descriptor heap is full");
		return kInvalidDescriptorIndex;
	}
	return m_currentOffset++;
}

CpuDescHandle DX12TransientDescriptorAllocator::GetCpuHandle(uint32_t descriptorIndex) const
{
	if (!m_impl || !m_impl->heap || descriptorIndex >= m_totalCount)
	{
		return { 0 };
	}
	return { m_cpuStart + static_cast<uint64_t>(descriptorIndex) * static_cast<uint64_t>(m_descriptorSize) };
}

GpuDescHandle DX12TransientDescriptorAllocator::GetGpuHandle(uint32_t descriptorIndex) const
{
	if (!m_impl || !m_impl->heap || descriptorIndex >= m_totalCount)
	{
		return { 0 };
	}
	return { m_gpuStart + static_cast<uint64_t>(descriptorIndex) * static_cast<uint64_t>(m_descriptorSize) };
}
