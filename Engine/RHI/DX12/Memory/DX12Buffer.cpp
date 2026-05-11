#include "DX12Buffer.h"
#include "Engine/RHI/DX12/Resource/ResourceImpl.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"


DX12Buffer::DX12Buffer(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice)
	: m_impl(std::make_unique<ResourceImpl>())
{
	// DirectX 12バッファの作成コードをここに記述
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	ComPtr<ID3D12Resource> buffer;
	if (bufferDesc.MemoryType == ERHIMemoryType::Upload)
	{
		auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferDesc.Size);
		auto result = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&buffer)
		);
		if (FAILED(result)) {
			LOG_FATAL("Failed to create upload buffer");
			return;
		}
		m_impl->resource = buffer;
	}
	if (bufferDesc.MemoryType == ERHIMemoryType::Default)
	{
		auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferDesc.Size);
		auto result = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&buffer)
		);
		if (FAILED(result)) {
			LOG_FATAL("Failed to create default buffer");
			return;
		}
	}
	m_impl->resource = buffer;

	m_gpuAddress = buffer->GetGPUVirtualAddress();
	m_size = bufferDesc.Size;
}
DX12Buffer::~DX12Buffer()
{
	if (m_impl->resource)
	{
		m_impl->resource->Release();
		m_impl->resource = nullptr;
	}
	if (m_impl->resource)
	{
		m_impl->resource->Release();
		m_impl->resource = nullptr;
	}
}

void* DX12Buffer::Map()
{
	if (m_impl->resource)
	{
		void* mappedData = nullptr;
		HRESULT hr = m_impl->resource->Map(0, nullptr, &mappedData);
		if (FAILED(hr))
		{
			LOG_ERROR("Failed to map upload resource");
			return nullptr;
		}
		return mappedData;
	}
	else
	{
		LOG_ERROR("Upload resource is not available for mapping");
		return nullptr;
	}
}

void DX12Buffer::Unmap()
{
	if (m_impl->resource)
	{
		m_impl->resource->Unmap(0, nullptr);
	}
	else
	{
		LOG_ERROR("Upload resource is not available for unmapping");
	}
}

void
DX12Buffer::TransitionResource(ERHIResourceState newState, const RHICommandList* rhiCommandList)
{
	m_impl->TransitionResource(newState, rhiCommandList);
}

DX12VertexBuffer::DX12VertexBuffer(const RHIBufferDesc& bufferDesc, uint32_t stride, const DX12Device* dxDevice)
	: m_buffer(std::make_unique<DX12Buffer>(bufferDesc, dxDevice)), m_stride(stride)
{
}

DX12VertexBuffer::~DX12VertexBuffer()
{
	m_buffer.release();
}

DX12IndexBuffer::DX12IndexBuffer(const RHIBufferDesc& bufferDesc, IndexFormat indexFormat, const DX12Device* dxDevice)
	: m_buffer(std::make_unique<DX12Buffer>(bufferDesc, dxDevice)), m_indexFormat(indexFormat)
{
}

DX12IndexBuffer::~DX12IndexBuffer()
{
	m_buffer.release();
}

DX12ConstantBuffer::DX12ConstantBuffer(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice)
	: m_buffer(std::make_unique<DX12Buffer>(bufferDesc, dxDevice))
{
}

DX12ConstantBuffer::~DX12ConstantBuffer()
{
	m_buffer.release();
}

DX12StructuredBuffer::DX12StructuredBuffer(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice)
	: m_buffer(std::make_unique<DX12Buffer>(bufferDesc, dxDevice))
{
}

DX12StructuredBuffer::~DX12StructuredBuffer()
{
	m_buffer.release();
}