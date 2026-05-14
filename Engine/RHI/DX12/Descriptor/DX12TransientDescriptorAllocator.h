#pragma once
<<<<<<< HEAD

#include <Engine/RHI/Interface/RHITransientDescriptorAllocator.h>

#include <cstdint>
#include <memory>
=======
#include "Engine/RHI/Interface/RHITransientDescriptorAllocator.h"
>>>>>>> c0092d2 (shaderSystemのヘッダーファイルを追加)

class DX12Device;
class HeapImpl;

class DX12TransientDescriptorAllocator final : public RHITransientDescriptorAllocator
{
public:
	~DX12TransientDescriptorAllocator() override;

	void Reset() override;
	uint32_t Allocate() override;
	CpuDescHandle GetCpuHandle(uint32_t descriptorIndex) const override;
	GpuDescHandle GetGpuHandle(uint32_t descriptorIndex) const override;

private:
	DX12TransientDescriptorAllocator(uint32_t numDescriptors, const DX12Device* dxDevice);

	static std::unique_ptr<DX12TransientDescriptorAllocator> Create(uint32_t numDescriptors, const DX12Device* dxDevice)
	{
		return std::unique_ptr<DX12TransientDescriptorAllocator>(new DX12TransientDescriptorAllocator(numDescriptors, dxDevice));
	}

	std::unique_ptr<HeapImpl> m_impl;
	uint32_t m_descriptorSize = 0;
	uint32_t m_totalCount = 0;
	uint32_t m_currentOffset = 0;
	uint64_t m_cpuStart = 0;
	uint64_t m_gpuStart = 0;

	friend class DX12Device;
};
