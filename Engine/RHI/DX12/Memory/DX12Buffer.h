#pragma once

#include <Engine/RHI/Interface/RHIBuffer.h>


class DX12Device;
class ResourceImpl;

class DX12Buffer
{
public:
	DX12Buffer(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice);
	~DX12Buffer();
	void* Map();
	void Unmap();
	size_t GetSize() const { return m_size; }
	uint64_t GetGPUAddress() const { return m_gpuAddress; }

	void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList);

private:


	size_t m_size = 0;
	uint64_t m_gpuAddress = 0;

	std::unique_ptr<ResourceImpl> m_impl;

	ResourceImpl* GetImpl() const { return m_impl.get(); }
};

class DX12VertexBuffer : public RHIVertexBuffer
{
public:
	~DX12VertexBuffer() override;
	void* Map() override { return m_buffer->Map(); }
	void Unmap() override { m_buffer->Unmap(); }
	size_t GetSize() const override { return m_buffer->GetSize(); }
	uint64_t GetGPUAddress() const override { return m_buffer->GetGPUAddress(); }
	uint32_t GetStride() const override { return m_stride; }
	void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList) override { m_buffer->TransitionResource(newState, commandList); }
private:

	DX12VertexBuffer(const RHIBufferDesc& bufferDesc, uint32_t stride, const DX12Device* dxDevice);

	static std::unique_ptr<DX12VertexBuffer> Create(const RHIBufferDesc& bufferDesc, uint32_t stride, const DX12Device* dxDevice) 
	{ 
		return std::make_unique<DX12VertexBuffer>(bufferDesc, stride, dxDevice); 
	}

	std::unique_ptr<DX12Buffer> m_buffer;

	uint32_t m_stride;

	friend class DX12Device;
};

class DX12IndexBuffer : public RHIIndexBuffer
{
public:
	~DX12IndexBuffer() override;
	void* Map() override { return m_buffer->Map(); }
	void Unmap() override { m_buffer->Unmap(); }
	size_t GetSize() const override { return m_buffer->GetSize(); }
	uint64_t GetGPUAddress() const override { return m_buffer->GetGPUAddress(); }
	IndexFormat GetIndexFormat() const override { return m_indexFormat; }
	void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList) override { m_buffer->TransitionResource(newState, commandList); }
private:

	DX12IndexBuffer(const RHIBufferDesc& bufferDesc, IndexFormat indexFormat, const DX12Device* dxDevice);

	static std::unique_ptr<DX12IndexBuffer> Create(const RHIBufferDesc& bufferDesc, IndexFormat indexFormat, const DX12Device* dxDevice) 
	{ 
		return std::make_unique<DX12IndexBuffer>(bufferDesc, indexFormat, dxDevice); 
	}

	std::unique_ptr<DX12Buffer> m_buffer;
	IndexFormat m_indexFormat;

	friend class DX12Device;
};

class DX12ConstantBuffer : public RHIConstantBuffer
{
public:
	~DX12ConstantBuffer() override;
	void* Map() override { return m_buffer->Map(); }
	void Unmap() override { m_buffer->Unmap(); }
	size_t GetSize() const override { return m_buffer->GetSize(); }
	uint64_t GetGPUAddress() const override { return m_buffer->GetGPUAddress(); }
	void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList) override { m_buffer->TransitionResource(newState, commandList); }

private:

	DX12ConstantBuffer(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice);

	static std::unique_ptr<DX12ConstantBuffer> Create(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice) 
	{ 
		return std::make_unique<DX12ConstantBuffer>(bufferDesc, dxDevice); 
	}

	std::unique_ptr<DX12Buffer> m_buffer;

	friend class DX12Device;
};


class DX12StructuredBuffer : public RHIStructuredBuffer
{
public:
	~DX12StructuredBuffer() override;
	void* Map() override { return m_buffer->Map(); }
	void Unmap() override { m_buffer->Unmap(); }
	size_t GetSize() const override { return m_buffer->GetSize(); }
	uint64_t GetGPUAddress() const override { return m_buffer->GetGPUAddress(); }
	void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList) override { m_buffer->TransitionResource(newState, commandList); }

private:

	DX12StructuredBuffer(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice);

	static std::unique_ptr<DX12StructuredBuffer> Create(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice) 
	{ 
		return std::make_unique<DX12StructuredBuffer>(bufferDesc, dxDevice); 
	}

	std::unique_ptr<DX12Buffer> m_buffer;

	friend class DX12Device;
};