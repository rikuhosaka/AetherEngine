#include "DX12RTVAllocator.h"

#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Descriptor/AllocatorImpl.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Debug/DX12GpuNaming.h"

#include <format>

bool DX12RTVAllocator::IsValid() const
{
	return m_impl != nullptr && m_impl->heap != nullptr;
}

Result<std::unique_ptr<DX12RTVAllocator>> DX12RTVAllocator::Create(
	uint32_t numDescriptors,
	const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12RTVAllocator>(new DX12RTVAllocator(numDescriptors, dxDevice)),
		"Failed to create RTV allocator");
}

DX12RTVAllocator::DX12RTVAllocator(uint32_t numDescriptors, const DX12Device* dxDevice)
	: m_currentOffset(0)
	, m_impl(std::make_unique<HeapImpl>())
{
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = numDescriptors;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	auto result = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));
	if (FAILED(result))
	{
		return;
	}
	m_impl->heap = descriptorHeap;
	m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_cpuStart = m_impl->heap->GetCPUDescriptorHandleForHeapStart().ptr;

	DX12GpuNaming::SetName(
		m_impl->heap.Get(),
		std::format("Heap/RTV/{}", numDescriptors));
}

DX12RTVAllocator::~DX12RTVAllocator()
{
	// ComPtr owns the COM object lifetime; avoid manual Release() (double-release).
	if (m_impl != nullptr)
	{
		m_impl->heap.Reset();
	}
}

RtvHandle DX12RTVAllocator::Allocate(uint32_t numDescriptors)
{
	RtvHandle handle;
	handle.cpu.ptr = m_cpuStart + m_currentOffset * m_descriptorSize;
	m_currentOffset += numDescriptors;
	return handle;
}
