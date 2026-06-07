#pragma once

#include <DirectXMath.h>

#include <cstdint>

namespace Aether::Math
{
[[nodiscard]] DirectX::XMMATRIX LookAt(
	DirectX::FXMVECTOR eye,
	DirectX::FXMVECTOR focus,
	DirectX::FXMVECTOR up);

[[nodiscard]] DirectX::XMMATRIX PerspectiveFovLH(
	float fovYRadians,
	float aspectRatio,
	float nearPlane,
	float farPlane);

[[nodiscard]] DirectX::XMMATRIX OrthographicLH(
	float viewWidth,
	float viewHeight,
	float nearPlane,
	float farPlane);

// Builds view and projection matrices from a yaw/pitch/position camera description.
struct YawPitchCameraParams
{
	DirectX::XMFLOAT3 position { 0.0f, 0.0f, 0.0f };
	float yawRadians = 0.0f;
	float pitchRadians = 0.0f;
	float fovYRadians = DirectX::XMConvertToRadians(60.0f);
	float nearPlane = 0.1f;
	float farPlane = 2000.0f;
};

struct YawPitchCameraMatrices
{
	float view[16]{};
	float projection[16]{};
	float viewProjection[16]{};
	DirectX::XMFLOAT3 cameraPosition { 0.0f, 0.0f, 0.0f };
};

void BuildYawPitchCameraMatrices(
	const YawPitchCameraParams& params,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	YawPitchCameraMatrices& outMatrices);
} // namespace Aether::Math
