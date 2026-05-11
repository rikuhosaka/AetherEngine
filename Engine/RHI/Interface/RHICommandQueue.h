#pragma once


class RHICommandList;
class RHIFence;

class RHICommandQueue
{
public:
	virtual void ExecuteCommandLists(const std::vector<RHICommandList*>& commandLists) = 0;
	virtual uint64_t Signal(RHIFence* fence) = 0;
	virtual void WaitGPU(RHIFence* fence, uint64_t value) = 0;

protected:
	RHICommandQueue() = default;
};