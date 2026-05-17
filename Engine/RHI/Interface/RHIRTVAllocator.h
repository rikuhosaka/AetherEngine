#pragma once
#include "Engine/RHI/Common/RHIDescriptor.h"


class RHIRTVAllocator
{
public:

	virtual RtvHandle Allocate(uint32_t numDescriptors = 1) = 0;
protected:
	RHIRTVAllocator() = default;
};