#pragma once

#include <Engine/RHI/Interface/RHICommon.h>

struct RHIDescriptorHandle {
	uint64_t cpu;
	uint64_t gpu;
};

class RHIDescriptorAllocator
{
public:

	virtual void BeginFrame(uint32_t frameIndex) = 0;

	virtual uint32_t Allocate() = 0;

	virtual CpuDescHandle GetCpuHandle(uint32_t descriptorIndex) = 0;

	virtual GpuDescHandle GetGpuHandle(uint32_t descriptorIndex) = 0;
};