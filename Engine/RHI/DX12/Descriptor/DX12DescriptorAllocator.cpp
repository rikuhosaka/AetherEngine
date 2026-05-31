#include "Engine/RHI/DX12/Descriptor/DX12DescriptorAllocator.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Descriptor/AllocatorImpl.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"

bool DX12DescriptorAllocator::IsValid() const
{
	return m_impl != nullptr && m_impl->heap != nullptr && m_totalCount > 0;
}

Result<std::unique_ptr<DX12DescriptorAllocator>> DX12DescriptorAllocator::Create(
	uint32_t numDescriptors,
	const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12DescriptorAllocator>(new DX12DescriptorAllocator(numDescriptors, dxDevice)),
		"Failed to create descriptor allocator");
}

DX12DescriptorAllocator::DX12DescriptorAllocator(uint32_t numDescriptors, const DX12Device* dxDevice)
	: m_impl(std::make_unique<HeapImpl>())
	, m_totalCount(numDescriptors)
{
	if (m_totalCount == 0)
	{
		LOG_FATAL(LogCategory::RHI, "Descriptor allocator requires non-zero descriptor count");
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

DX12DescriptorAllocator::~DX12DescriptorAllocator() = default;

void DX12DescriptorAllocator::BeginFrame(uint32_t frameIndex)
{
	m_frameIndex = frameIndex;
	m_currentOffset = frameIndex * m_frameSize;
	m_frameAllocations.clear();
}

uint32_t DX12DescriptorAllocator::Allocate()
{
	if (!m_impl || !m_impl->heap || m_totalCount == 0)
	{
		LOG_FATAL(LogCategory::RHI, "Descriptor allocator heap is not initialized");
		return UINT32_MAX;
	}
	if (m_frameSize == 0)
	{
		m_frameSize = m_totalCount / FrameCount;
	}
	if (m_currentOffset >= (m_frameIndex + 1) * m_frameSize)
	{
		LOG_ERROR(LogCategory::RHI, "Descriptor allocator frame heap is full");
		return UINT32_MAX;
	}

	const uint32_t index = m_currentOffset++;
	CbvSrvUavHandle handle{};
	handle.cpu.ptr = m_cpuStart + static_cast<uint64_t>(index) * static_cast<uint64_t>(m_descriptorSize);
	handle.gpu.ptr = m_gpuStart + static_cast<uint64_t>(index) * static_cast<uint64_t>(m_descriptorSize);
	m_frameAllocations.emplace(index, handle);
	return index;
}
