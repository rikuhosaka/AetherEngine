#include "Runtime/Engine/EngineInstance/Test/CameraViewBuildTest.h"

#include "Runtime/Engine/Camera/CameraState.h"
#include "Runtime/Engine/EngineInstance/CameraViewBuild.h"
#include "Runtime/Core/Log/LogMacros.h"

#include <cmath>

namespace
{
[[nodiscard]] bool IsNearlyEqual(float left, float right)
{
	return std::fabs(left - right) <= 0.0001f;
}

[[nodiscard]] bool SameExtractedView(const ExtractedView& left, const ExtractedView& right)
{
	if (left.viewportWidth != right.viewportWidth || left.viewportHeight != right.viewportHeight)
	{
		return false;
	}

	for (int index = 0; index < 3; ++index)
	{
		if (!IsNearlyEqual(left.cameraPosition[index], right.cameraPosition[index]))
		{
			return false;
		}
	}

	for (int index = 0; index < 16; ++index)
	{
		if (!IsNearlyEqual(left.viewMatrix[index], right.viewMatrix[index])
			|| !IsNearlyEqual(left.projectionMatrix[index], right.projectionMatrix[index])
			|| !IsNearlyEqual(left.viewProjectionMatrix[index], right.viewProjectionMatrix[index]))
		{
			return false;
		}
	}

	return true;
}
} // namespace

Result<void> RunCameraViewBuildTests()
{
	CameraState camera{};
	camera.position = { 0.0f, 1.6f, -4.0f };
	camera.forward = { 0.0f, 0.0f, 1.0f };
	camera.up = { 0.0f, 1.0f, 0.0f };

	ExtractedView view{};
	BuildExtractedView(camera, 640, 480, view);
	ExtractedView wideLens = view;
	camera.lens.fovYDegrees = 90.0f;
	camera.lens.nearPlane = 1.0f;
	camera.lens.farPlane = 100.0f;
	BuildExtractedView(camera, 640, 480, wideLens);

	if (view.viewportWidth != 640 || view.viewportHeight != 480
		|| !IsNearlyEqual(view.cameraPosition[0], 0.0f)
		|| !IsNearlyEqual(view.cameraPosition[1], 1.6f)
		|| !IsNearlyEqual(view.cameraPosition[2], -4.0f)
		|| SameExtractedView(view, wideLens))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"BuildExtractedView should write the viewport, camera position, and lens");
	}

	LOG_INFO(LogCategory::Renderer, "CameraViewBuild tests passed");
	return MakeOk();
}
