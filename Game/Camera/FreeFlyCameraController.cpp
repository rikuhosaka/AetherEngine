#include "Game/Camera/FreeFlyCameraController.h"

#include "Engine/Platform/InputManager.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>

namespace
{
DirectX::XMFLOAT3 ForwardFromYawPitch(float yawRadians, float pitchRadians)
{
	const float cosPitch = std::cosf(pitchRadians);
	return {
		std::sinf(yawRadians) * cosPitch,
		std::sinf(pitchRadians),
		std::cosf(yawRadians) * cosPitch,
	};
}

void YawPitchFromForward(const DirectX::XMFLOAT3& forward, float& outYawRadians, float& outPitchRadians)
{
	using namespace DirectX;

	const XMVECTOR direction = XMVector3Normalize(XMLoadFloat3(&forward));
	XMFLOAT3 normalized{};
	XMStoreFloat3(&normalized, direction);

	outPitchRadians = std::asinf(std::clamp(normalized.y, -1.0f, 1.0f));
	outYawRadians = std::atan2f(normalized.x, normalized.z);
}
} // namespace

void FreeFlyCameraController::Reset(CameraState& camera)
{
	using namespace DirectX;

	camera.position = { 0.0f, 2.0f, -5.0f };
	camera.up = { 0.0f, 1.0f, 0.0f };
	camera.lens = CameraLens{};

	const XMVECTOR eye = XMLoadFloat3(&camera.position);
	const XMVECTOR focus = XMVectorZero();
	const XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(focus, eye));
	XMStoreFloat3(&camera.forward, forward);

	YawPitchFromForward(camera.forward, m_yawRadians, m_pitchRadians);
}

void FreeFlyCameraController::Update(
	CameraState& camera,
	const InputManager& input,
	float deltaSeconds)
{
	using namespace DirectX;

	if (input.IsMouseButtonDown(VK_RBUTTON))
	{
		const MousePixelPosition mouseDelta = input.GetMouseDeltaPixels();
		m_yawRadians += static_cast<float>(mouseDelta.x) * m_settings.lookSensitivity;
		m_pitchRadians -= static_cast<float>(mouseDelta.y) * m_settings.lookSensitivity;
		m_pitchRadians = std::clamp(
			m_pitchRadians,
			m_settings.minPitchRadians,
			m_settings.maxPitchRadians);
		SyncOrientationFromYawPitch(camera);
	}

	XMVECTOR moveDirection = XMVectorZero();
	const XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&camera.forward));
	const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));

	if (input.IsKeyPressed('W'))
	{
		moveDirection = XMVectorAdd(moveDirection, forward);
	}
	if (input.IsKeyPressed('S'))
	{
		moveDirection = XMVectorSubtract(moveDirection, forward);
	}
	if (input.IsKeyPressed('D'))
	{
		moveDirection = XMVectorAdd(moveDirection, right);
	}
	if (input.IsKeyPressed('A'))
	{
		moveDirection = XMVectorSubtract(moveDirection, right);
	}
	if (input.IsKeyPressed(VK_SPACE))
	{
		moveDirection = XMVectorAdd(moveDirection, worldUp);
	}
	if (input.IsKeyPressed(VK_CONTROL))
	{
		moveDirection = XMVectorSubtract(moveDirection, worldUp);
	}

	const float moveLengthSq = XMVectorGetX(XMVector3LengthSq(moveDirection));
	if (moveLengthSq > 0.0f)
	{
		moveDirection = XMVector3Normalize(moveDirection);

		float moveSpeed = m_settings.moveSpeed;
		if (input.IsKeyPressed(VK_SHIFT))
		{
			moveSpeed *= m_settings.fastMoveScale;
		}

		XMVECTOR position = XMLoadFloat3(&camera.position);
		position = XMVectorAdd(position, XMVectorScale(moveDirection, moveSpeed * deltaSeconds));
		XMStoreFloat3(&camera.position, position);
	}
}

void FreeFlyCameraController::SyncOrientationFromYawPitch(CameraState& camera)
{
	camera.forward = ForwardFromYawPitch(m_yawRadians, m_pitchRadians);

	using namespace DirectX;
	const XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&camera.forward));
	const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR up = XMVector3Cross(forward, worldUp);
	up = XMVector3Normalize(up);
	up = XMVector3Cross(up, forward);
	XMStoreFloat3(&camera.up, up);
}
