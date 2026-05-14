#pragma once
#include "Engine/RHI/Interface/RHIResource.h"

struct RHIUploadAllocation
{
	void* cpuAddress = nullptr;
	uint64_t gpuVirtualAddress = 0;
	size_t offset = 0;
	size_t size = 0;
};

<<<<<<< HEAD
class RHIUploadBuffer
=======
class RHIUploadBuffer : public RHIResource
>>>>>>> c0092d2 (shaderSystemのヘッダーファイルを追加)
{
public:
	virtual ~RHIUploadBuffer() = default;

	virtual void Reset() = 0;

	virtual RHIUploadAllocation Allocate(size_t size, size_t alignment) = 0;
};
