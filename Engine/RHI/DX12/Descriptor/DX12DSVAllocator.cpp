#include "DX12DSVAllocator.h"

#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Descriptor/AllocatorImpl.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"

bool DX12DSVAllocator::IsValid() const
{
	return m_impl != nullptr && m_impl->heap != nullptr;
}

Result<std::unique_ptr<DX12DSVAllocator>> DX12DSVAllocator::Create(
	uint32_t numDescriptors,
	const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12DSVAllocator>(new DX12DSVAllocator(numDescriptors, dxDevice)),
		"Failed to create DSV allocator");
}

DX12DSVAllocator::DX12DSVAllocator(uint32_t numDescriptors, const DX12Device* dxDevice)
	: m_currentOffset(0)
	, m_impl(std::make_unique<HeapImpl>())
{
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = numDescriptors;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	auto result = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));
	if (FAILED(result))
	{
		return;
	}
	m_impl->heap = descriptorHeap;
	m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_cpuStart = m_impl->heap->GetCPUDescriptorHandleForHeapStart().ptr;
}

DX12DSVAllocator::~DX12DSVAllocator()
{
	if (m_impl->heap)
	{
		m_impl->heap->Release();
		m_impl->heap = nullptr;
	}
}

DsvHandle DX12DSVAllocator::Allocate(uint32_t numDescriptors)
{
	DsvHandle handle;
	handle.cpu.ptr = m_cpuStart + m_currentOffset * m_descriptorSize;
	m_currentOffset += numDescriptors;
	return handle;
}
