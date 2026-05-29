#pragma once

#include "Engine/RHI/Common/RHIDescriptor.h"

#include <cstdint>

class RHICommandList;
class RHIUploadBuffer;
class RHITexture;
class RHITransientDescriptorAllocator;

struct FrameContext
{
	uint32_t frameIndex = 0;
	uint64_t fenceValue = 0;

	RHICommandList* graphicsCommandList = nullptr;
	RHIUploadBuffer* uploadBuffer = nullptr;
	RHITransientDescriptorAllocator* transientDescriptors = nullptr;

	RtvHandle backBufferRtv{};
	DsvHandle depthDsv{};
	RHITexture* backBuffer = nullptr;
	RHITexture* depthTexture = nullptr;

	uint32_t backBufferIndex = 0;
	uint32_t renderWidth = 0;
	uint32_t renderHeight = 0;
};
