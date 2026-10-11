#include "Engine/Application/Test/FixedStepTest.h"

#include "Engine/Application/Subsystem/FrameClock.h"
#include "Engine/Application/Subsystem/SubsystemTypes.h"
#include "Engine/Core/Log/LogMacros.h"

#include <cmath>

namespace
{
[[nodiscard]] bool IsNear(float left, float right, float epsilon = 0.001f)
{
	return std::fabs(left - right) <= epsilon;
}
} // namespace

Result<void> RunFixedStepTests()
{
	double accumulator = 0.0;
	const FixedStepAdvance fiftyMilliseconds = AdvanceFixedSteps(
		accumulator,
		0.05f,
		EngineConstants::kFixedDeltaSeconds,
		EngineConstants::kMaxFixedStepsPerFrame);
	if (fiftyMilliseconds.stepCount != 3 || fiftyMilliseconds.alpha > 0.001f)
	{
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"A 50ms frame should advance three fixed steps");
	}

	const float leftoverFrame =
		EngineConstants::kFixedDeltaSeconds * 3.0f + EngineConstants::kFixedDeltaSeconds * 0.05f;
	const FixedStepAdvance withRemainder = AdvanceFixedSteps(
		accumulator,
		leftoverFrame,
		EngineConstants::kFixedDeltaSeconds,
		EngineConstants::kMaxFixedStepsPerFrame);
	if (withRemainder.stepCount != 3 || !IsNear(withRemainder.alpha, 0.05f))
	{
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Three fixed steps should leave an interpolation alpha of about 0.05");
	}

	const double beforePartial = accumulator;
	const float partialFrame = EngineConstants::kFixedDeltaSeconds * 0.25f;
	const FixedStepAdvance partial = AdvanceFixedSteps(
		accumulator,
		partialFrame,
		EngineConstants::kFixedDeltaSeconds,
		EngineConstants::kMaxFixedStepsPerFrame);
	if (partial.stepCount != 0 || accumulator <= beforePartial)
	{
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"A frame shorter than the fixed step should accumulate without stepping");
	}

	LOG_INFO(LogCategory::Core, "FixedStepTests passed");
	return MakeOk();
}
