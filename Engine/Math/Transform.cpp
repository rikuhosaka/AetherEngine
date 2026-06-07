#include "Engine/Math/Transform.h"

namespace Aether::Math
{
using namespace DirectX;

XMMATRIX Translation(FXMVECTOR translation)
{
	return XMMatrixTranslationFromVector(translation);
}

XMMATRIX RotationRollPitchYaw(float pitchRadians, float yawRadians, float rollRadians)
{
	return XMMatrixRotationRollPitchYaw(pitchRadians, yawRadians, rollRadians);
}

XMMATRIX RotationQuaternion(FXMVECTOR quaternion)
{
	return XMMatrixRotationQuaternion(quaternion);
}

XMMATRIX Scale(FXMVECTOR scale)
{
	return XMMatrixScalingFromVector(scale);
}

XMMATRIX TRS(FXMVECTOR translation, FXMVECTOR rotationQuaternion, FXMVECTOR scale)
{
	const XMMATRIX translationMatrix = Translation(translation);
	const XMMATRIX rotationMatrix = RotationQuaternion(rotationQuaternion);
	const XMMATRIX scaleMatrix = Scale(scale);
	return XMMatrixMultiply(XMMatrixMultiply(scaleMatrix, rotationMatrix), translationMatrix);
}
} // namespace Aether::Math
