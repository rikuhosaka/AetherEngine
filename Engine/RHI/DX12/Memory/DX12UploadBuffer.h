#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHIUploadBuffer.h"

#include <memory>

class ResourceImpl;
class DX12Device;

class DX12UploadBuffer final : public RHIUploadBuffer
{
public:
	~DX12UploadBuffer() override;

	void Reset() override;
	RHIUploadAllocation Allocate(size_t size, size_t alignment) override;

	void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList) override;

	[[nodiscard]] bool IsValid() const;

private:
	explicit DX12UploadBuffer(size_t capacityInBytes, const DX12Device* dxDevice);

	static Result<std::unique_ptr<DX12UploadBuffer>> Create(size_t capacityInBytes, const DX12Device* dxDevice);

	std::unique_ptr<ResourceImpl> m_impl;

	ResourceImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12CommandList;

	void* m_mappedBase = nullptr;
	uint64_t m_gpuVirtualAddress = 0;
	size_t m_capacity = 0;
	size_t m_cursor = 0;

	friend class DX12Device;
};
