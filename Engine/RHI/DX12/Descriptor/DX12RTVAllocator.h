#pragma once
#include "Engine/RHI/Interface/RHIRTVAllocator.h"


class DX12Device;
class HeapImpl;

class DX12RTVAllocator : public RHIRTVAllocator
{

public:
	~DX12RTVAllocator();
	virtual RtvHandle Allocate(uint32_t numDescriptors = 1) override;

private:

	DX12RTVAllocator(uint32_t numDescriptors, const DX12Device* dxDevice);

	uint32_t m_descriptorSize;
	uint32_t m_currentOffset;
	size_t m_cpuStart;

	std::unique_ptr<HeapImpl> m_impl = nullptr;

	HeapImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12Device;
};