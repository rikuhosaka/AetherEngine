#pragma once

#include "Runtime/Engine/Application/Subsystem/SubsystemTypes.h"
#include "Runtime/Renderer/Resource/FrameContext.h"

#include <array>
#include <cstdint>

class RHIBarrierDebug;
class RHIDevice;
class RHICommandQueue;
class RHIFence;

struct RHIServices
{
	RHIDevice* device = nullptr;
	RHICommandQueue* graphicsQueue = nullptr;
	RHIFence* frameFence = nullptr;
	RHIBarrierDebug* barrierDebug = nullptr;

	static constexpr uint32_t kFrameCount = EngineConstants::kFrameInFlightCount;

	std::array<FrameContext, kFrameCount> frameContexts{};

	uint32_t currentFrameSlot = 0;
};
