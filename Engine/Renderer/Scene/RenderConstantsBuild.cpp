#include "Engine/Renderer/Scene/RenderConstantsBuild.h"

#include "Engine/Math/Matrix.h"
#include "Engine/Renderer/Scene/RenderLightingTypes.h"
#include "Engine/Renderer/Scene/RenderViewTypes.h"
#include "Engine/Renderer/Scene/ShadowMapConstants.h"

#include <DirectXMath.h>
#include <cmath>
#include <cstring>

namespace
{
void CopyMatrix(float destination[16], const float source[16])
{
	std::memcpy(destination, source, sizeof(float) * 16);
}

void NormalizeDirection(float outDirection[3], const float direction[3])
{
	using namespace DirectX;

	const XMVECTOR vector = XMVectorSet(direction[0], direction[1], direction[2], 0.0f);
	const XMVECTOR normalized = XMVector3Normalize(vector);
	XMFLOAT3 result{};
	XMStoreFloat3(&result, normalized);
	outDirection[0] = result.x;
	outDirection[1] = result.y;
	outDirection[2] = result.z;
}
} // namespace

void BuildFrameConstants(
	const ExtractedView& view,
	const ExtractedLighting& lighting,
	FrameConstants& outConstants)
{
	CopyMatrix(outConstants.viewMatrix, view.viewMatrix);
	CopyMatrix(outConstants.projectionMatrix, view.projectionMatrix);
	CopyMatrix(outConstants.viewProjectionMatrix, view.viewProjectionMatrix);

	outConstants.cameraPosition[0] = view.cameraPosition[0];
	outConstants.cameraPosition[1] = view.cameraPosition[1];
	outConstants.cameraPosition[2] = view.cameraPosition[2];
	outConstants.cameraPosition[3] = 0.0f;

	outConstants.ambientColor[0] = lighting.ambientColor[0];
	outConstants.ambientColor[1] = lighting.ambientColor[1];
	outConstants.ambientColor[2] = lighting.ambientColor[2];
	outConstants.ambientColor[3] = lighting.ambientIntensity;

	float normalizedDirection[3]{};
	NormalizeDirection(normalizedDirection, lighting.mainLightDirection);
	outConstants.mainLightDirection[0] = normalizedDirection[0];
	outConstants.mainLightDirection[1] = normalizedDirection[1];
	outConstants.mainLightDirection[2] = normalizedDirection[2];
	outConstants.mainLightDirection[3] = 0.0f;

	outConstants.mainLightColor[0] = lighting.mainLightColor[0];
	outConstants.mainLightColor[1] = lighting.mainLightColor[1];
	outConstants.mainLightColor[2] = lighting.mainLightColor[2];
	outConstants.mainLightColor[3] = lighting.mainLightIntensity;

	using namespace DirectX;

	const XMVECTOR lightDirection = XMVector3Normalize(XMVectorSet(
		outConstants.mainLightDirection[0],
		outConstants.mainLightDirection[1],
		outConstants.mainLightDirection[2],
		0.0f));
	const XMVECTOR sceneCenter = XMVectorSet(0.0f, kShadowSceneCenterY, 0.0f, 1.0f);
	const XMVECTOR eye = XMVectorSubtract(
		sceneCenter,
		XMVectorScale(lightDirection, kShadowSceneRadius * 2.0f));
	const float directionY = XMVectorGetY(lightDirection);
	const XMVECTOR up = std::fabs(directionY) > 0.99f
		? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
		: XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const XMMATRIX lightView = XMMatrixLookToLH(eye, lightDirection, up);
	const float orthoSize = kShadowSceneRadius * 2.0f;
	const XMMATRIX lightProjection = XMMatrixOrthographicLH(
		orthoSize,
		orthoSize,
		kShadowSceneRadius * 0.5f,
		kShadowSceneRadius * 4.0f);
	const XMMATRIX lightViewProjection = XMMatrixMultiply(lightView, lightProjection);
	Aether::Math::StoreMatrixForHlsl(outConstants.lightViewProjection, lightViewProjection);

	outConstants.shadowParams[0] = kShadowDepthBias;
	outConstants.shadowParams[1] = 1.0f / static_cast<float>(kShadowMapSize);
	outConstants.shadowParams[2] = static_cast<float>(kShadowMapSize);
	outConstants.shadowParams[3] = 0.0f;
}

void BuildObjectConstants(const float worldMatrix[16], ObjectConstants& outConstants)
{
	CopyMatrix(outConstants.worldMatrix, worldMatrix);

	const DirectX::XMMATRIX world = Aether::Math::LoadMatrixFromHlsl(worldMatrix);
	const DirectX::XMMATRIX worldInverseTranspose = Aether::Math::InverseTranspose(world);
	Aether::Math::StoreMatrixForHlsl(outConstants.worldInverseTranspose, worldInverseTranspose);
}
