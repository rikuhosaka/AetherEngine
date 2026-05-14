#pragma once
#include "Engine/RHI/Interface/RHICommon.h"

class RHITransientDescriptorAllocator
{
public:
	virtual ~RHITransientDescriptorAllocator() = default;

	virtual void Reset() = 0;

	virtual uint32_t Allocate() = 0;

	virtual CpuDescHandle GetCpuHandle(uint32_t descriptorIndex) const = 0;
	virtual GpuDescHandle GetGpuHandle(uint32_t descriptorIndex) const = 0;
};
