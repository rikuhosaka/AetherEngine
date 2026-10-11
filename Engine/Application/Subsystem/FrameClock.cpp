#include "Engine/Application/Subsystem/FrameClock.h"

#include "Engine/Application/Subsystem/SubsystemTypes.h"

FixedStepAdvance AdvanceFixedSteps(
	double& accumulatorSeconds,
	float frameDeltaSeconds,
	float fixedDeltaSeconds,
	uint32_t maxSteps)
{
	FixedStepAdvance advance{};
	if (fixedDeltaSeconds <= 0.0f || maxSteps == 0)
	{
		return advance;
	}

	if (frameDeltaSeconds > 0.0f)
	{
		accumulatorSeconds += static_cast<double>(frameDeltaSeconds);
	}

	const double stepSeconds = static_cast<double>(fixedDeltaSeconds);
	// A 50ms frame is three 1/60 steps, but the float values fall short by a few ulps.
	const double stepTolerance = stepSeconds * 1.0e-4;
	while (accumulatorSeconds + stepTolerance >= stepSeconds && advance.stepCount < maxSteps)
	{
		accumulatorSeconds -= stepSeconds;
		if (accumulatorSeconds < 0.0)
		{
			accumulatorSeconds = 0.0;
		}
		++advance.stepCount;
	}

	float alpha = static_cast<float>(accumulatorSeconds / stepSeconds);
	if (alpha < 0.0f)
	{
		alpha = 0.0f;
	}
	else if (alpha >= 1.0f)
	{
		alpha = 0.999999f;
	}
	advance.alpha = alpha;
	return advance;
}

void FrameClock::Reset()
{
	QueryPerformanceFrequency(&m_frequency);
	QueryPerformanceCounter(&m_lastCounter);
	m_hasLastCounter = true;
	m_deltaSeconds = 0.0f;
	m_unclampedDeltaSeconds = 0.0f;
	m_totalSeconds = 0.0;
}

float FrameClock::Tick(float maxDeltaSeconds)
{
	if (!m_hasLastCounter)
	{
		Reset();
		return EngineConstants::kMinDeltaSeconds;
	}

	LARGE_INTEGER currentCounter{};
	QueryPerformanceCounter(&currentCounter);

	const double rawDeltaSeconds = static_cast<double>(currentCounter.QuadPart - m_lastCounter.QuadPart)
		/ static_cast<double>(m_frequency.QuadPart);

	m_lastCounter = currentCounter;
	m_unclampedDeltaSeconds = static_cast<float>(rawDeltaSeconds);
	m_deltaSeconds = m_unclampedDeltaSeconds < maxDeltaSeconds
		? m_unclampedDeltaSeconds
		: maxDeltaSeconds;
	m_totalSeconds += static_cast<double>(m_deltaSeconds);
	return m_deltaSeconds;
}
