#pragma once

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

	// 同時に飛行させるフレーム数。スワップチェイン枚数とは別に持つ。
	static constexpr uint32_t kFrameCount = 3;

	std::array<FrameContext, kFrameCount> frameContexts{};

	uint32_t currentFrameSlot = 0;
};
