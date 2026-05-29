#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHIDescriptorAllocator.h"

#include <memory>

class DX12Device;
class HeapImpl;

class DX12DescriptorAllocator : public RHIDescriptorAllocator
{
public:
	~DX12DescriptorAllocator();
	void BeginFrame(uint32_t frameIndex) override;
	uint32_t Allocate() override;

	CpuDescHandle GetCpuHandle(uint32_t descriptorIndex) override
	{
		if (m_frameAllocations.find(descriptorIndex) != m_frameAllocations.end())
		{
			return m_frameAllocations[descriptorIndex].cpu;
		}
		return { 0 };
	}

	GpuDescHandle GetGpuHandle(uint32_t descriptorIndex) override
	{
		if (m_frameAllocations.find(descriptorIndex) != m_frameAllocations.end())
		{
			return m_frameAllocations[descriptorIndex].gpu;
		}
		return { 0 };
	}

	[[nodiscard]] bool IsValid() const;

private:
	DX12DescriptorAllocator(uint32_t numDescriptors, const DX12Device* dxDevice);

	static Result<std::unique_ptr<DX12DescriptorAllocator>> Create(
		uint32_t numDescriptors,
		const DX12Device* dxDevice);

	std::unordered_map<uint32_t, CbvSrvUavHandle> m_frameAllocations;

	uint32_t m_descriptorSize = 0;
	uint32_t m_totalCount = 0;

	uint32_t m_currentOffset = 0;

	uint32_t m_frameIndex = 0;
	uint32_t m_frameSize = 0;

	static const uint32_t FrameCount = 3;

	size_t m_cpuStart;
	uint64_t m_gpuStart;

	std::unique_ptr<HeapImpl> m_impl = nullptr;

	HeapImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12Device;
	friend class DX12CommandList;
};
