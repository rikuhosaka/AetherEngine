#pragma once

#include <cstddef>
#include <cstdint>

struct RHIUploadAllocation
{
	void* cpuAddress = nullptr;
	uint64_t gpuVirtualAddress = 0;
	size_t offset = 0;
	size_t size = 0;
};

class RHIUploadBuffer
{
public:
	virtual ~RHIUploadBuffer() = default;

	virtual void Reset() = 0;

	virtual RHIUploadAllocation Allocate(size_t size, size_t alignment) = 0;
};
