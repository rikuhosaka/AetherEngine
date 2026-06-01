#pragma once

#include "Engine/RHI/Common/RHIResource.h"
#include "Engine/RHI/Common/RHIInput.h"
#include "Engine/RHI/Interface/RHIBuffer.h"
#include "Engine/Core/Log/Result.h"

#include <string_view>

class DX12Device;
class ResourceImpl;

class DX12Buffer
{
public:
	~DX12Buffer();
	void* Map();
	void Unmap();
	size_t GetSize() const { return m_size; }
	uint64_t GetGPUAddress() const { return m_gpuAddress; }

	void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList);

	[[nodiscard]] ResourceImpl* GetResourceImpl() const { return m_impl.get(); }
	[[nodiscard]] bool IsValid() const;

private:
	DX12Buffer(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice, std::string_view namePrefix);

	size_t m_size = 0;
	uint64_t m_gpuAddress = 0;

	std::unique_ptr<ResourceImpl> m_impl;

	friend class DX12CommandList;
	friend class DX12Device;
	friend class DX12VertexBuffer;
	friend class DX12IndexBuffer;
	friend class DX12ConstantBuffer;
	friend class DX12StructuredBuffer;
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
	void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList) override
	{
		m_buffer->TransitionResource(newState, commandList);
	}

	[[nodiscard]] bool IsValid() const;

private:
	DX12VertexBuffer(const RHIBufferDesc& bufferDesc, uint32_t stride, const DX12Device* dxDevice);

	static Result<std::unique_ptr<DX12VertexBuffer>> Create(
		const RHIBufferDesc& bufferDesc,
		uint32_t stride,
		const DX12Device* dxDevice);

	std::unique_ptr<DX12Buffer> m_buffer;
	uint32_t m_stride;

	[[nodiscard]] ResourceImpl* GetBufferResourceImpl() const { return m_buffer->GetResourceImpl(); }

	friend class DX12Device;
	friend class DX12CommandList;
};

class DX12IndexBuffer : public RHIIndexBuffer
{
public:
	~DX12IndexBuffer() override;
	void* Map() override { return m_buffer->Map(); }
	void Unmap() override { return m_buffer->Unmap(); }
	size_t GetSize() const override { return m_buffer->GetSize(); }
	uint64_t GetGPUAddress() const override { return m_buffer->GetGPUAddress(); }
	IndexFormat GetIndexFormat() const override { return m_indexFormat; }
	void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList) override
	{
		m_buffer->TransitionResource(newState, commandList);
	}

	[[nodiscard]] bool IsValid() const;

private:
	DX12IndexBuffer(const RHIBufferDesc& bufferDesc, IndexFormat indexFormat, const DX12Device* dxDevice);

	static Result<std::unique_ptr<DX12IndexBuffer>> Create(
		const RHIBufferDesc& bufferDesc,
		IndexFormat indexFormat,
		const DX12Device* dxDevice);

	std::unique_ptr<DX12Buffer> m_buffer;
	IndexFormat m_indexFormat;

	[[nodiscard]] ResourceImpl* GetBufferResourceImpl() const { return m_buffer->GetResourceImpl(); }

	friend class DX12Device;
	friend class DX12CommandList;
};

class DX12ConstantBuffer : public RHIConstantBuffer
{
public:
	~DX12ConstantBuffer() override;
	void* Map() override { return m_buffer->Map(); }
	void Unmap() override { return m_buffer->Unmap(); }
	size_t GetSize() const override { return m_buffer->GetSize(); }
	uint64_t GetGPUAddress() const override { return m_buffer->GetGPUAddress(); }
	void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList) override
	{
		m_buffer->TransitionResource(newState, commandList);
	}

	[[nodiscard]] bool IsValid() const;

private:
	DX12ConstantBuffer(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice);

	static Result<std::unique_ptr<DX12ConstantBuffer>> Create(
		const RHIBufferDesc& bufferDesc,
		const DX12Device* dxDevice);

	std::unique_ptr<DX12Buffer> m_buffer;

	[[nodiscard]] ResourceImpl* GetBufferResourceImpl() const { return m_buffer->GetResourceImpl(); }

	friend class DX12Device;
	friend class DX12CommandList;
};

class DX12StructuredBuffer : public RHIStructuredBuffer
{
public:
	~DX12StructuredBuffer() override;
	void* Map() override { return m_buffer->Map(); }
	void Unmap() override { return m_buffer->Unmap(); }
	size_t GetSize() const override { return m_buffer->GetSize(); }
	uint64_t GetGPUAddress() const override { return m_buffer->GetGPUAddress(); }
	void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList) override
	{
		m_buffer->TransitionResource(newState, commandList);
	}

	[[nodiscard]] bool IsValid() const;

private:
	DX12StructuredBuffer(const RHIBufferDesc& bufferDesc, const DX12Device* dxDevice);

	static Result<std::unique_ptr<DX12StructuredBuffer>> Create(
		const RHIBufferDesc& bufferDesc,
		const DX12Device* dxDevice);

	std::unique_ptr<DX12Buffer> m_buffer;

	[[nodiscard]] ResourceImpl* GetBufferResourceImpl() const { return m_buffer->GetResourceImpl(); }

	friend class DX12Device;
	friend class DX12CommandList;
};
