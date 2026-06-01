#include "DX12Buffer.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Resource/ResourceImpl.h"
#include "Engine/RHI/DX12/Debug/DX12GpuNaming.h"

bool DX12Buffer::IsValid() const
{
	return m_impl != nullptr && m_impl->resource != nullptr;
}

DX12Buffer::DX12Buffer(
	const RHIBufferDesc& bufferDesc,
	const DX12Device* dxDevice,
	std::string_view namePrefix)
	: m_impl(std::make_unique<ResourceImpl>())
{
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	ComPtr<ID3D12Resource> buffer;
	if (bufferDesc.MemoryType == ERHIMemoryType::Upload)
	{
		const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferDesc.Size);
		const HRESULT result = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&buffer));
		if (FAILED(result))
		{
			return;
		}
		m_impl->resource = buffer;
	}
	else if (bufferDesc.MemoryType == ERHIMemoryType::Default)
	{
		const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferDesc.Size);
		const HRESULT result = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&buffer));
		if (FAILED(result))
		{
			return;
		}
		m_impl->resource = buffer;
	}

	if (!IsValid())
	{
		return;
	}

	m_gpuAddress = buffer->GetGPUVirtualAddress();
	m_size = bufferDesc.Size;
	DX12GpuNaming::SetResourceName(m_impl->resource.Get(), namePrefix, bufferDesc.DebugName);
}

DX12Buffer::~DX12Buffer()
{
	if (m_impl && m_impl->resource)
	{
		m_impl->resource.Reset();
	}
}

void* DX12Buffer::Map()
{
	if (m_impl->resource)
	{
		void* mappedData = nullptr;
		const HRESULT hr = m_impl->resource->Map(0, nullptr, &mappedData);
		if (FAILED(hr))
		{
			LOG_ERROR(LogCategory::RHI, "Failed to map upload resource");
			return nullptr;
		}
		return mappedData;
	}

	LOG_ERROR(LogCategory::RHI, "Upload resource is not available for mapping");
	return nullptr;
}

void DX12Buffer::Unmap()
{
	if (m_impl->resource)
	{
		m_impl->resource->Unmap(0, nullptr);
	}
	else
	{
		LOG_ERROR(LogCategory::RHI, "Upload resource is not available for unmapping");
	}
}

void DX12Buffer::TransitionResource(ERHIResourceState newState, const RHICommandList* rhiCommandList)
{
	m_impl->TransitionResource(newState, rhiCommandList);
}

Result<std::unique_ptr<DX12VertexBuffer>> DX12VertexBuffer::Create(
	const RHIBufferDesc& bufferDesc,
	uint32_t stride,
	const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12VertexBuffer>(new DX12VertexBuffer(bufferDesc, stride, dxDevice)),
		"Failed to create vertex buffer");
}

bool DX12VertexBuffer::IsValid() const
{
	return m_buffer != nullptr && m_buffer->IsValid();
}

DX12VertexBuffer::DX12VertexBuffer(
	const RHIBufferDesc& bufferDesc,
	uint32_t stride,
	const DX12Device* dxDevice)
	: m_buffer(std::unique_ptr<DX12Buffer>(new DX12Buffer(bufferDesc, dxDevice, "VB")))
	, m_stride(stride)
{
}

DX12VertexBuffer::~DX12VertexBuffer() = default;

Result<std::unique_ptr<DX12IndexBuffer>> DX12IndexBuffer::Create(
	const RHIBufferDesc& bufferDesc,
	IndexFormat indexFormat,
	const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12IndexBuffer>(new DX12IndexBuffer(bufferDesc, indexFormat, dxDevice)),
		"Failed to create index buffer");
}

bool DX12IndexBuffer::IsValid() const
{
	return m_buffer != nullptr && m_buffer->IsValid();
}

DX12IndexBuffer::DX12IndexBuffer(
	const RHIBufferDesc& bufferDesc,
	IndexFormat indexFormat,
	const DX12Device* dxDevice)
	: m_buffer(std::unique_ptr<DX12Buffer>(new DX12Buffer(bufferDesc, dxDevice, "IB")))
	, m_indexFormat(indexFormat)
{
}

DX12IndexBuffer::~DX12IndexBuffer() = default;

Result<std::unique_ptr<DX12ConstantBuffer>> DX12ConstantBuffer::Create(
	const RHIBufferDesc& bufferDesc,
	const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12ConstantBuffer>(new DX12ConstantBuffer(bufferDesc, dxDevice)),
		"Failed to create constant buffer");
}

bool DX12ConstantBuffer::IsValid() const
{
	return m_buffer != nullptr && m_buffer->IsValid();
}

DX12ConstantBuffer::DX12ConstantBuffer(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice)
	: m_buffer(std::unique_ptr<DX12Buffer>(new DX12Buffer(bufferDesc, dxDevice, "CB")))
{
}

DX12ConstantBuffer::~DX12ConstantBuffer() = default;

Result<std::unique_ptr<DX12StructuredBuffer>> DX12StructuredBuffer::Create(
	const RHIBufferDesc& bufferDesc,
	const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12StructuredBuffer>(new DX12StructuredBuffer(bufferDesc, dxDevice)),
		"Failed to create structured buffer");
}

bool DX12StructuredBuffer::IsValid() const
{
	return m_buffer != nullptr && m_buffer->IsValid();
}

DX12StructuredBuffer::DX12StructuredBuffer(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice)
	: m_buffer(std::unique_ptr<DX12Buffer>(new DX12Buffer(bufferDesc, dxDevice, "SB")))
{
}

DX12StructuredBuffer::~DX12StructuredBuffer() = default;
