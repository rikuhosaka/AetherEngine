#pragma once
#include "Engine/RHI/Interface/RHICommon.h"


class RHIRTVAllocator
{
public:

	virtual RtvHandle Allocate(uint32_t numDescriptors = 1) = 0;
protected:
	RHIRTVAllocator() = default;
};