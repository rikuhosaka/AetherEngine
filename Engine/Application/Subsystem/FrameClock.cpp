#include "Engine/Application/Subsystem/FrameClock.h"

#include "Engine/Application/Subsystem/SubsystemTypes.h"

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
