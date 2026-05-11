#pragma once


class RHIFence
{
public:
	virtual ~RHIFence() = default;
	virtual void Increment() = 0;
	virtual bool IsComplete(uint64_t value) = 0;
	virtual void WaitCPU(uint64_t value) = 0;

protected:
	RHIFence() = default;
};