#pragma once

#include "Game/Camera/CameraState.h"

class InputManager;

struct FreeFlyCameraSettings
{
	float moveSpeed = 8.0f;
	float fastMoveScale = 4.0f;
	float lookSensitivity = 0.002f;
	float minPitchRadians = -DirectX::XM_PIDIV2 + 0.01f;
	float maxPitchRadians = DirectX::XM_PIDIV2 - 0.01f;
};

class FreeFlyCameraController
{
public:
	void SetSettings(const FreeFlyCameraSettings& settings) { m_settings = settings; }
	[[nodiscard]] const FreeFlyCameraSettings& GetSettings() const noexcept { return m_settings; }

	void Reset(CameraState& camera);
	void Reset(CameraState& camera, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& forward);
	void Update(CameraState& camera, const InputManager& input, float deltaSeconds);

private:
	void SyncOrientationFromYawPitch(CameraState& camera);

	FreeFlyCameraSettings m_settings{};
	float m_yawRadians = 0.0f;
	float m_pitchRadians = 0.0f;
};
