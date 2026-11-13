#pragma once

#include <cstdint>

enum class SubsystemTickGroup : int32_t
{
	Input = 100,
	Ui = 150,
	EditorUi = 160,
	UiPresent = 170,
	Game = 200,
	PreRender = 300,
};

enum class SubsystemState : uint8_t
{
	Registered,
	Initialized,
	Running,
	Shutdown,
};

namespace EngineConstants
{
constexpr uint32_t kFrameInFlightCount = 3;
constexpr uint32_t kSwapChainBufferCount = 3;

constexpr float kMaxDeltaSeconds = 0.1f;
constexpr float kMinDeltaSeconds = 0.0f;
constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
constexpr uint32_t kMaxFixedStepsPerFrame = 8;
} // namespace EngineConstants
