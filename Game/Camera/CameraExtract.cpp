#include "Game/Camera/CameraExtract.h"

#include "Engine/Math/Camera.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Renderer/Scene/RenderViewTypes.h"

void BuildExtractedView(
	const CameraState& camera,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	ExtractedView& outView)
{
	using namespace DirectX;

	outView = ExtractedView{};
	outView.viewportWidth = viewportWidth;
	outView.viewportHeight = viewportHeight;

	const XMVECTOR eye = XMLoadFloat3(&camera.position);
	const XMVECTOR focus = XMVectorAdd(eye, XMLoadFloat3(&camera.forward));
	const XMVECTOR up = XMLoadFloat3(&camera.up);

	const float aspectRatio = viewportHeight > 0
		? static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight)
		: 1.0f;

	const XMMATRIX view = Aether::Math::LookAt(eye, focus, up);
	const XMMATRIX projection = Aether::Math::PerspectiveFovLH(
		XMConvertToRadians(camera.lens.fovYDegrees),
		aspectRatio,
		camera.lens.nearPlane,
		camera.lens.farPlane);
	const XMMATRIX viewProjection = XMMatrixMultiply(view, projection);

	Aether::Math::StoreMatrixForHlsl(outView.viewMatrix, view);
	Aether::Math::StoreMatrixForHlsl(outView.projectionMatrix, projection);
	Aether::Math::StoreMatrixForHlsl(outView.viewProjectionMatrix, viewProjection);

	XMFLOAT3 cameraPosition{};
	XMStoreFloat3(&cameraPosition, eye);
	outView.cameraPosition[0] = cameraPosition.x;
	outView.cameraPosition[1] = cameraPosition.y;
	outView.cameraPosition[2] = cameraPosition.z;
}
