#pragma once

#include <DirectXMath.h>

namespace Aether::Math
{
[[nodiscard]] DirectX::XMMATRIX Translation(DirectX::FXMVECTOR translation);

[[nodiscard]] DirectX::XMMATRIX RotationRollPitchYaw(
	float pitchRadians,
	float yawRadians,
	float rollRadians);

[[nodiscard]] DirectX::XMMATRIX RotationQuaternion(DirectX::FXMVECTOR quaternion);

[[nodiscard]] DirectX::XMMATRIX Scale(DirectX::FXMVECTOR scale);

[[nodiscard]] DirectX::XMMATRIX TRS(
	DirectX::FXMVECTOR translation,
	DirectX::FXMVECTOR rotationQuaternion,
	DirectX::FXMVECTOR scale);
} // namespace Aether::Math
