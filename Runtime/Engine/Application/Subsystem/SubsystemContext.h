#pragma once

#include <cstdint>

// 責任: そのフレームの時刻をサブシステムへ渡す。
// 役割: 可変刻み、補間係数、フレームスロットの置き場。
// 所有: EngineLoop のメンバ。
class SubsystemContext
{
public:
	void SetDeltaSeconds(float deltaSeconds) { m_deltaSeconds = deltaSeconds; }
	[[nodiscard]] float GetDeltaSeconds() const { return m_deltaSeconds; }

	void SetInterpolationAlpha(float alpha) { m_interpolationAlpha = alpha; }
	[[nodiscard]] float GetInterpolationAlpha() const { return m_interpolationAlpha; }

	void SetFrameSlot(uint32_t frameSlot) { m_frameSlot = frameSlot; }
	[[nodiscard]] uint32_t GetFrameSlot() const { return m_frameSlot; }

private:
	float m_deltaSeconds = 0.0f;
	float m_interpolationAlpha = 0.0f;
	uint32_t m_frameSlot = 0;
};
