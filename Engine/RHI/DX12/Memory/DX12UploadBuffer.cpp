#include "Engine/RHI/DX12/Memory/DX12UploadBuffer.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"

namespace
{
	size_t AlignUp(size_t value, size_t alignment)
	{
		if (alignment < 1)
		{
			alignment = 1;
		}
		const size_t rem = value % alignment;
		return rem == 0 ? value : value + (alignment - rem);
	}
}

DX12UploadBuffer::DX12UploadBuffer(size_t capacityInBytes, const DX12Device* dxDevice)
	: m_capacity(capacityInBytes)
	, m_cursor(0)
{
	if (m_capacity == 0)
	{
		return;
	}

	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_capacity);

	const HRESULT hr = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_resource));
	if (FAILED(hr))
	{
		LOG_FATAL("Failed to create upload ring buffer");
		return;
	}

	m_gpuVirtualAddress = m_resource->GetGPUVirtualAddress();

	const HRESULT mapHr = m_resource->Map(0, nullptr, &m_mappedBase);
	if (FAILED(mapHr))
	{
		LOG_FATAL("Failed to map upload ring buffer");
		m_resource.Reset();
		m_gpuVirtualAddress = 0;
	}
}

DX12UploadBuffer::~DX12UploadBuffer()
{
	if (m_resource && m_mappedBase)
	{
		m_resource->Unmap(0, nullptr);
		m_mappedBase = nullptr;
	}
	m_resource.Reset();
}

void DX12UploadBuffer::Reset()
{
	m_cursor = 0;
}

RHIUploadAllocation DX12UploadBuffer::Allocate(size_t size, size_t alignment)
{
	if (size == 0 || m_mappedBase == nullptr || m_resource == nullptr || m_capacity == 0)
	{
		return {};
	}

	const size_t alignedOffset = AlignUp(m_cursor, alignment);
	if (alignedOffset > m_capacity || size > m_capacity - alignedOffset)
	{
		LOG_ERROR("Upload ring buffer out of memory");
		return {};
	}

	RHIUploadAllocation allocation{};
	allocation.cpuAddress = reinterpret_cast<char*>(m_mappedBase) + alignedOffset;
	allocation.gpuVirtualAddress = m_gpuVirtualAddress + alignedOffset;
	allocation.offset = alignedOffset;
	allocation.size = size;

	m_cursor = alignedOffset + size;
	return allocation;
}
