#include "Engine/Renderer/Test/ShadowMapTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Renderer/Scene/RenderConstantsBuild.h"
#include "Engine/Renderer/Scene/RenderLightingTypes.h"
#include "Engine/Renderer/Scene/RenderSceneDefaults.h"
#include "Engine/Renderer/Scene/RenderViewTypes.h"
#include "Engine/Renderer/Scene/ShadowMapConstants.h"

#include <DirectXMath.h>

#include <cmath>

namespace
{
struct ShadowProjection
{
	float uvX = 0.0f;
	float uvY = 0.0f;
	float depth = 0.0f;
};

[[nodiscard]] bool IsNearlyEqual(float left, float right, float epsilon = 0.0001f)
{
	return std::fabs(left - right) <= epsilon;
}

// Matches LitBasicPS.hlsl SampleShadow: mul(float4(worldPos, 1), lightViewProjection),
// then uv = ndc.xy * (0.5, -0.5) + 0.5 and depth = ndc.z.
[[nodiscard]] ShadowProjection ProjectToShadowMap(const FrameConstants& constants, const float worldPosition[3])
{
	using namespace DirectX;

	const XMMATRIX lightViewProjection = Aether::Math::LoadMatrixFromHlsl(constants.lightViewProjection);
	const XMVECTOR world = XMVectorSet(worldPosition[0], worldPosition[1], worldPosition[2], 1.0f);
	const XMVECTOR clip = XMVector3TransformCoord(world, lightViewProjection);

	ShadowProjection projection{};
	projection.uvX = XMVectorGetX(clip) * 0.5f + 0.5f;
	projection.uvY = XMVectorGetY(clip) * -0.5f + 0.5f;
	projection.depth = XMVectorGetZ(clip);
	return projection;
}

// Matches LitBasicPS.hlsl: receiverDepth <= closestDepth + bias is lit.
[[nodiscard]] bool IsLitByShadowTest(float receiverDepth, float closestDepth, float bias)
{
	return receiverDepth <= closestDepth + bias;
}

[[nodiscard]] bool IsInsideShadowMap(const ShadowProjection& projection)
{
	return projection.uvX >= 0.0f && projection.uvX <= 1.0f
		&& projection.uvY >= 0.0f && projection.uvY <= 1.0f;
}

[[nodiscard]] Result<void> ExpectInside(const ShadowProjection& projection, const char* message)
{
	if (!IsInsideShadowMap(projection) || projection.depth <= 0.0f || projection.depth >= 1.0f)
	{
		return FailRuntime(LogCategory::Renderer, ErrorCode::InvalidArgument, message);
	}

	return MakeOk();
}

[[nodiscard]] Result<void> ExpectShadowParams(const FrameConstants& constants)
{
	if (!IsNearlyEqual(constants.shadowParams[0], kShadowDepthBias)
		|| !IsNearlyEqual(constants.shadowParams[1], 1.0f / static_cast<float>(kShadowMapSize))
		|| !IsNearlyEqual(constants.shadowParams[2], static_cast<float>(kShadowMapSize))
		|| !IsNearlyEqual(constants.shadowParams[3], 0.0f))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Shadow params do not match the shadow map constants");
	}

	return MakeOk();
}
} // namespace

Result<void> RunShadowMapTests()
{
	ExtractedView view{};
	ExtractedLighting lighting{};
	InitDefaultExtractedView(view, 1280, 720);
	InitDefaultExtractedLighting(lighting);

	FrameConstants constants{};
	BuildFrameConstants(view, lighting, constants);
	if (auto paramsResult = ExpectShadowParams(constants); !paramsResult)
	{
		return paramsResult;
	}

	const float roomCenter[3] = { 0.0f, kShadowSceneCenterY, 0.0f };
	const ShadowProjection center = ProjectToShadowMap(constants, roomCenter);
	if (!IsNearlyEqual(center.uvX, 0.5f, 0.02f) || !IsNearlyEqual(center.uvY, 0.5f, 0.02f)
		|| center.depth <= 0.0f || center.depth >= 1.0f)
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Room center should project near the middle of the shadow map");
	}

	const float floorOrigin[3] = { 0.0f, 0.0f, 0.0f };
	if (auto floorResult = ExpectInside(ProjectToShadowMap(constants, floorOrigin),
			"Floor origin should lie inside the shadow map");
		!floorResult)
	{
		return floorResult;
	}

	const float outside[3] = { 100.0f, kShadowSceneCenterY, 0.0f };
	if (IsInsideShadowMap(ProjectToShadowMap(constants, outside)))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"A point far outside the room should fall outside the shadow map");
	}

	lighting.mainLightDirection[0] = 0.0f;
	lighting.mainLightDirection[1] = -1.0f;
	lighting.mainLightDirection[2] = 0.0f;
	BuildFrameConstants(view, lighting, constants);
	if (auto overheadResult = ExpectInside(ProjectToShadowMap(constants, roomCenter),
			"Room center should stay inside the shadow map for a straight-down light");
		!overheadResult)
	{
		return overheadResult;
	}

	constexpr float kClosestDepth = 0.6f;
	if (!IsLitByShadowTest(0.4f, kClosestDepth, kShadowDepthBias))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"A receiver closer than the shadow map should be lit");
	}

	if (IsLitByShadowTest(0.8f, kClosestDepth, kShadowDepthBias))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"A receiver farther than the shadow map should be in shadow");
	}

	if (!IsLitByShadowTest(kClosestDepth + kShadowDepthBias * 0.5f, kClosestDepth, kShadowDepthBias))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"A depth difference smaller than the shadow bias should stay lit");
	}

	LOG_INFO(LogCategory::Renderer, "ShadowMapTests passed");
	return MakeOk();
}
