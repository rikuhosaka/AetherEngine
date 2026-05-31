#pragma once

#include <Windows.h>

class FrameClock
{
public:
	void Reset();
	float Tick(float maxDeltaSeconds);

	[[nodiscard]] float GetUnclampedDeltaSeconds() const { return m_unclampedDeltaSeconds; }
	[[nodiscard]] float GetDeltaSeconds() const { return m_deltaSeconds; }
	[[nodiscard]] double GetTotalSeconds() const { return m_totalSeconds; }

private:
	LARGE_INTEGER m_frequency{};
	LARGE_INTEGER m_lastCounter{};
	float m_deltaSeconds = 0.0f;
	float m_unclampedDeltaSeconds = 0.0f;
	double m_totalSeconds = 0.0;
	bool m_hasLastCounter = false;
};
