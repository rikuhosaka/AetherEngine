#include "Engine/Math/Camera.h"

#include "Engine/Math/Matrix.h"

#include <cmath>

namespace Aether::Math
{
using namespace DirectX;

XMMATRIX LookAt(FXMVECTOR eye, FXMVECTOR focus, FXMVECTOR up)
{
	return XMMatrixLookAtLH(eye, focus, up);
}

XMMATRIX PerspectiveFovLH(float fovYRadians, float aspectRatio, float nearPlane, float farPlane)
{
	return XMMatrixPerspectiveFovLH(fovYRadians, aspectRatio, nearPlane, farPlane);
}

XMMATRIX OrthographicLH(float viewWidth, float viewHeight, float nearPlane, float farPlane)
{
	return XMMatrixOrthographicLH(viewWidth, viewHeight, nearPlane, farPlane);
}

void BuildYawPitchCameraMatrices(
	const YawPitchCameraParams& params,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	YawPitchCameraMatrices& outMatrices)
{
	const XMVECTOR eye = XMLoadFloat3(&params.position);

	const float cosPitch = std::cosf(params.pitchRadians);
	const XMFLOAT3 forward{
		std::sinf(params.yawRadians) * cosPitch,
		std::sinf(params.pitchRadians),
		std::cosf(params.yawRadians) * cosPitch,
	};

	const XMVECTOR focus = XMVectorAdd(eye, XMLoadFloat3(&forward));
	const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	const float aspectRatio = viewportHeight > 0
		? static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight)
		: 1.0f;

	const XMMATRIX view = LookAt(eye, focus, up);
	const XMMATRIX projection = PerspectiveFovLH(
		params.fovYRadians,
		aspectRatio,
		params.nearPlane,
		params.farPlane);
	const XMMATRIX viewProjection = XMMatrixMultiply(view, projection);

	StoreMatrixForHlsl(outMatrices.view, view);
	StoreMatrixForHlsl(outMatrices.projection, projection);
	StoreMatrixForHlsl(outMatrices.viewProjection, viewProjection);
	outMatrices.cameraPosition = params.position;
}
} // namespace Aether::Math
