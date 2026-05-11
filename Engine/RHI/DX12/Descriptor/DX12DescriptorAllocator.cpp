#include "DX12DescriptorAllocator.h"
#include "Engine/RHI/DX12/Descriptor/AllocatorImpl.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"


DX12DescriptorAllocator::DX12DescriptorAllocator(uint32_t numDescriptors, const DX12Device* dxDevice)
	: 
	m_frameIndex(0),
	m_currentOffset(0),
	m_frameSize(numDescriptors / FrameCount),
	m_totalCount(numDescriptors),
	m_impl(std::make_unique<HeapImpl>())
{
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = numDescriptors;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	auto result = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));
	if (FAILED(result)) {
		LOG_FATAL("Failed to create descriptor heap");
		return;
	}
	m_impl->heap = descriptorHeap;
	m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_cpuStart = m_impl->heap->GetCPUDescriptorHandleForHeapStart().ptr;
	m_gpuStart = m_impl->heap->GetGPUDescriptorHandleForHeapStart().ptr;
}

DX12DescriptorAllocator::~DX12DescriptorAllocator()
{
	if (m_impl->heap)
	{
		m_impl->heap->Release();
		m_impl->heap = nullptr;
	}
}

void DX12DescriptorAllocator::BeginFrame(uint32_t frameIndex)
{
	
	m_frameIndex = frameIndex;
	m_currentOffset = m_frameIndex * m_frameSize;
}

uint32_t
DX12DescriptorAllocator::Allocate()
{
	if (m_currentOffset + 1 > m_totalCount) {
		// エラー処理: デスクリプタが足りない
		LOG_ERROR("Descriptor heap is full. Cannot allocate more descriptors.");
		return 0;
	}
	uint32_t descriptorIndex = m_currentOffset;
	CbvSrvUavHandle handle;
	handle.cpu.ptr = m_cpuStart + descriptorIndex * m_descriptorSize;
	handle.gpu.ptr = m_gpuStart + descriptorIndex * m_descriptorSize;

	m_frameAllocations[descriptorIndex] = handle;
	m_currentOffset++;

	return descriptorIndex;
}