#pragma once
#include "Engine/RHI/Common/RHIDescriptor.h"


class RHIDSVAllocator
{
public:
	virtual DsvHandle Allocate(uint32_t numDescriptors = 1) = 0;
protected:
	RHIDSVAllocator() = default;
};
