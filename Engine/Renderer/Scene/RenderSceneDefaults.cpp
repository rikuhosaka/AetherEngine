#include "Engine/Renderer/Scene/RenderSceneDefaults.h"

#include "Engine/Math/Camera.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Renderer/Scene/RenderLightingTypes.h"
#include "Engine/Renderer/Scene/RenderViewTypes.h"

#include <DirectXMath.h>
#include <cstring>

void InitDefaultExtractedView(ExtractedView& outView, uint32_t viewportWidth, uint32_t viewportHeight)
{
	using namespace DirectX;

	outView = ExtractedView{};
	outView.viewportWidth = viewportWidth;
	outView.viewportHeight = viewportHeight;

	const XMVECTOR eye = XMVectorSet(0.0f, 2.0f, -5.0f, 0.0f);
	const XMVECTOR focus = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	const float aspectRatio = viewportHeight > 0
		? static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight)
		: 1.0f;

	const XMMATRIX view = Aether::Math::LookAt(eye, focus, up);
	const XMMATRIX projection = Aether::Math::PerspectiveFovLH(
		XMConvertToRadians(60.0f),
		aspectRatio,
		0.1f,
		2000.0f);
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

void InitDefaultExtractedLighting(ExtractedLighting& outLighting)
{
	outLighting = ExtractedLighting{};
}
