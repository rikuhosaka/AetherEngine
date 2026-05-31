#pragma once

#include "Engine/Application/Subsystem/SubsystemTypes.h"
#include "Engine/Frame/FrameContext.h"

#include <array>
#include <cstdint>

class RHIDevice;
class RHICommandQueue;
class RHIFence;
class RHICommandList;
class RHIUploadBuffer;
class RHITransientDescriptorAllocator;

struct RHIServices
{
	RHIDevice* device = nullptr;
	RHICommandQueue* graphicsQueue = nullptr;
	RHIFence* frameFence = nullptr;

	static constexpr uint32_t kFrameCount = EngineConstants::kFrameInFlightCount;

	std::array<FrameContext, kFrameCount> frameContexts{};
	std::array<RHICommandList*, kFrameCount> commandLists{};
	std::array<RHIUploadBuffer*, kFrameCount> uploadBuffers{};
	std::array<RHITransientDescriptorAllocator*, kFrameCount> transientAllocators{};

	uint32_t currentFrameSlot = 0;
};
