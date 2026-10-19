#pragma once

#include "Engine/Camera/CameraState.h"
#include "Engine/Camera/FreeFlyCameraController.h"

#include <cstdint>

class InputState;
struct EditorCameraServices;

class EditorCamera
{
public:
	void Reset();
	void Update(const InputState* input, float deltaSeconds, bool useMouse, bool useKeyboard);
	void Publish(uint32_t viewportWidth, uint32_t viewportHeight, EditorCameraServices& out) const;

private:
	static constexpr DirectX::XMFLOAT3 kStartPosition{ 0.0f, 1.6f, -4.0f };
	static constexpr DirectX::XMFLOAT3 kStartForward{ 0.0f, 0.0f, 1.0f };

	CameraState m_state{};
	FreeFlyCameraController m_controller{};
};
