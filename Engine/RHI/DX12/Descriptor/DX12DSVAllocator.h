#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHIDSVAllocator.h"

#include <memory>

class DX12Device;
class HeapImpl;

class DX12DSVAllocator : public RHIDSVAllocator
{
public:
	~DX12DSVAllocator();
	DsvHandle Allocate(uint32_t numDescriptors = 1) override;

	[[nodiscard]] bool IsValid() const;

private:
	DX12DSVAllocator(uint32_t numDescriptors, const DX12Device* dxDevice);

	static Result<std::unique_ptr<DX12DSVAllocator>> Create(uint32_t numDescriptors, const DX12Device* dxDevice);

	uint32_t m_descriptorSize;
	uint32_t m_currentOffset;
	size_t m_cpuStart;

	std::unique_ptr<HeapImpl> m_impl = nullptr;

	HeapImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12Device;
};
