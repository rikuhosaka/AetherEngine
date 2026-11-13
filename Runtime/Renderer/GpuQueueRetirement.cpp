#include "Runtime/Renderer/GpuQueueRetirement.h"

#include "Runtime/Renderer/RHIServices.h"
#include "Runtime/RHI/Interface/RHICommandList.h"
#include "Runtime/RHI/Interface/RHICommandQueue.h"
#include "Runtime/RHI/Interface/RHIFence.h"

namespace
{
void ReleaseCommandListReferences(RHICommandList* commandList)
{
	if (commandList == nullptr)
	{
		return;
	}

	// A closed list keeps every resource it recorded until Reset. Destroying
	// one of those resources and then executing the list is invalid.
	if (commandList->IsRecording())
	{
		commandList->Close();
	}

	commandList->Reset();
	commandList->Close();
}
} // namespace

void RetireGraphicsQueue(RHIServices* services)
{
	if (services == nullptr)
	{
		return;
	}

	if (services->frameFence != nullptr)
	{
		for (FrameContext& frameContext : services->frameContexts)
		{
			if (frameContext.fenceValue != 0)
			{
				services->frameFence->WaitCPU(frameContext.fenceValue);
				frameContext.fenceValue = 0;
			}
		}
	}

	if (services->graphicsQueue != nullptr)
	{
		services->graphicsQueue->WaitForIdle();
	}

	for (FrameContext& frameContext : services->frameContexts)
	{
		ReleaseCommandListReferences(frameContext.graphicsCommandList);
	}
}
