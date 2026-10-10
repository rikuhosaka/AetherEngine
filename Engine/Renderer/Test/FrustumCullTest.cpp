#include "Engine/Renderer/Test/FrustumCullTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Math/Camera.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Math/Transform.h"
#include "Engine/Renderer/Scene/FrustumCull.h"

#include <DirectXMath.h>

namespace
{
void StoreIdentity(float matrix[16])
{
	Aether::Math::StoreMatrixForHlsl(matrix, DirectX::XMMatrixIdentity());
}

void StoreTranslation(float matrix[16], float x, float y, float z)
{
	const DirectX::XMMATRIX translation = Aether::Math::TRS(
		DirectX::XMVectorSet(x, y, z, 0.0f),
		DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
		DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f));
	Aether::Math::StoreMatrixForHlsl(matrix, translation);
}
} // namespace

Result<void> RunFrustumCullTests()
{
	Aether::Math::YawPitchCameraParams camera{};
	camera.position = { 0.0f, 0.0f, 0.0f };
	camera.yawRadians = 0.0f;
	camera.pitchRadians = 0.0f;
	camera.fovYRadians = DirectX::XMConvertToRadians(60.0f);
	camera.nearPlane = 0.1f;
	camera.farPlane = 100.0f;

	Aether::Math::YawPitchCameraMatrices matrices{};
	Aether::Math::BuildYawPitchCameraMatrices(camera, 100, 100, matrices);

	float identity[16]{};
	StoreIdentity(identity);
	const ViewFrustum frustum = MakeViewFrustum(matrices.viewProjection);

	MeshBounds inFront{};
	inFront.minX = -0.5f;
	inFront.maxX = 0.5f;
	inFront.minY = -0.5f;
	inFront.maxY = 0.5f;
	inFront.minZ = 4.0f;
	inFront.maxZ = 6.0f;
	if (!IntersectsViewFrustum(frustum, identity, inFront))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"A box in front of the camera should stay in the frustum");
	}

	MeshBounds behind{};
	behind.minX = -0.5f;
	behind.maxX = 0.5f;
	behind.minY = -0.5f;
	behind.maxY = 0.5f;
	behind.minZ = -6.0f;
	behind.maxZ = -4.0f;
	if (IntersectsViewFrustum(frustum, identity, behind))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"A box behind the camera should be outside the frustum");
	}

	MeshBounds unit{};
	unit.minX = unit.minY = unit.minZ = -0.5f;
	unit.maxX = unit.maxY = unit.maxZ = 0.5f;
	float translatedForward[16]{};
	StoreTranslation(translatedForward, 0.0f, 0.0f, 5.0f);
	float translatedBackward[16]{};
	StoreTranslation(translatedBackward, 0.0f, 0.0f, -5.0f);
	if (!IntersectsViewFrustum(frustum, translatedForward, unit) ||
		IntersectsViewFrustum(frustum, translatedBackward, unit))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Frustum culling should use the object world translation");
	}

	MeshBounds invalid{};
	invalid.minX = 1.0f;
	invalid.maxX = -1.0f;
	if (AreBoundsCullable(invalid) || !AreBoundsCullable(unit))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Only bounds with min <= max on every axis can be culled");
	}

	LOG_INFO(LogCategory::Renderer, "FrustumCullTests passed");
	return MakeOk();
}
