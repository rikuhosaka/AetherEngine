#include "Engine/RHI/DX12/Memory/DX12UploadBuffer.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Resource/ResourceImpl.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Debug/DX12GpuNaming.h"

#include <atomic>
#include <format>

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

bool DX12UploadBuffer::IsValid() const
{
	return m_impl != nullptr && m_impl->resource != nullptr && m_mappedBase != nullptr;
}

Result<std::unique_ptr<DX12UploadBuffer>> DX12UploadBuffer::Create(
	size_t capacityInBytes,
	const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12UploadBuffer>(new DX12UploadBuffer(capacityInBytes, dxDevice)),
		"Failed to create upload buffer");
}

DX12UploadBuffer::DX12UploadBuffer(size_t capacityInBytes, const DX12Device* dxDevice)
	: m_capacity(capacityInBytes)
	, m_cursor(0)
	, m_impl(std::make_unique<ResourceImpl>())
{
	if (m_capacity == 0)
	{
		LOG_FATAL(LogCategory::RHI, "Upload buffer capacity must be non-zero");
		return;
	}

	ID3D12Device* device = dxDevice->GetImpl()->device.Get();

	ComPtr<ID3D12Resource> resource;

	const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_capacity);

	const HRESULT hr = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource));
	if (FAILED(hr))
	{
		return;
	}

	m_gpuVirtualAddress = resource->GetGPUVirtualAddress();

	const HRESULT mapHr = resource->Map(0, nullptr, &m_mappedBase);
	if (FAILED(mapHr))
	{
		LOG_ERROR(LogCategory::RHI, "Failed to map upload ring buffer");
		resource.Reset();
		m_gpuVirtualAddress = 0;
		return;
	}
	m_impl->resource = resource;
	m_impl->SetInitialState(ERHIResourceState::Common);

	static std::atomic<uint32_t> s_instanceCounter = 0;
	const uint32_t instanceId = s_instanceCounter.fetch_add(1, std::memory_order_relaxed);
	DX12GpuNaming::SetName(resource.Get(), std::format("Upload/Ring/{}", instanceId));
}

DX12UploadBuffer::~DX12UploadBuffer()
{
	if (m_impl->resource && m_mappedBase)
	{
		m_impl->resource->Unmap(0, nullptr);
		m_mappedBase = nullptr;
	}
	m_impl->resource.Reset();
}

void DX12UploadBuffer::TransitionResource(ERHIResourceState newState, const RHICommandList* commandList)
{
	if (m_impl->resource)
	{
		m_impl->TransitionResource(newState, commandList);
	}
}

void DX12UploadBuffer::Reset()
{
	m_cursor = 0;
}

RHIUploadAllocation DX12UploadBuffer::Allocate(size_t size, size_t alignment)
{
	if (size == 0 || m_mappedBase == nullptr || m_impl->resource == nullptr || m_capacity == 0)
	{
		return {};
	}

	const size_t alignedOffset = AlignUp(m_cursor, alignment);
	if (alignedOffset > m_capacity || size > m_capacity - alignedOffset)
	{
		LOG_ERROR(LogCategory::RHI, "Upload ring buffer out of memory");
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
