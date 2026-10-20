#include "Runtime/Launch/DebugStartupTests.h"

#include "Engine/Application/Test/FixedStepTest.h"
#include "Engine/Application/Test/SubsystemOrderTest.h"
#include "Engine/Renderer/Test/FbxModelFileLoaderTest.h"
#include "Engine/Renderer/Test/FrustumCullTest.h"
#include "Engine/Renderer/Test/PrimitiveMeshTest.h"
#include "Engine/Renderer/Test/RendererResourceSmokeTest.h"
#include "Engine/Renderer/Test/ShaderBytecodeLoaderTest.h"
#include "Engine/Renderer/Test/ShadowMapTest.h"
#include "Engine/Renderer/Test/TextureFileLoaderTest.h"
#include "Engine/Scene/SceneDocument.h"
#include "Engine/Scene/SceneDocumentTest.h"
#include "Engine/World/Test/EntityPhysicsTest.h"
#include "Engine/Physics/Test/PhysicsLibraryTest.h"
#include "Engine/Physics/Test/PhysicsWorldTest.h"
#include "Engine/World/Test/WorldTest.h"
#include "Engine/Scene/SceneCaptureTest.h"
#include "Engine/Scene/ServiceLifetimeTest.h"
#include "Engine/Scene/WorldExtractTest.h"

#include <cmath>
#include <string>

namespace
{
[[nodiscard]] bool RoundTripNearlyEqual(float left, float right)
{
	return std::fabs(left - right) <= 0.0001f;
}

[[nodiscard]] SceneDocument MakeRoundTripScene()
{
	SceneDocument document{};

	WorldDocumentEntity sphereWorld{};
	sphereWorld.id = 1;
	sphereWorld.name = "Sphere";
	sphereWorld.transform.position = { 0.0f, 4.0f, 0.0f };
	SceneEntity sphere{};
	sphere.id = 1;
	sphere.mesh = "builtin:sphere";
	sphere.tint[0] = 0.90f;
	sphere.tint[1] = 0.35f;
	sphere.tint[2] = 0.12f;
	sphere.tint[3] = 1.0f;
	RigidBody sphereBody{};
	sphereBody.motion = PhysicsMotion::Dynamic;
	sphereBody.friction = 0.2f;
	sphereBody.shape.type = PhysicsShapeType::Sphere;
	sphereBody.shape.radius = 0.5f;
	sphere.body = sphereBody;
	CameraComponent sphereCamera{};
	sphereCamera.fovYDegrees = 50.0f;
	sphereCamera.nearPlane = 0.2f;
	sphereCamera.farPlane = 500.0f;
	sphere.camera = sphereCamera;
	LightComponent sphereLight{};
	sphereLight.color[0] = 0.1f;
	sphereLight.color[1] = 0.2f;
	sphereLight.color[2] = 0.3f;
	sphereLight.intensity = 2.0f;
	sphere.light = sphereLight;

	WorldDocumentEntity hullWorld{};
	hullWorld.id = 2;
	hullWorld.name = "Hull";
	SceneEntity hull{};
	hull.id = 2;
	hull.mesh = "builtin:hull";
	RigidBody hullBody{};
	hullBody.motion = PhysicsMotion::Dynamic;
	hullBody.shape.type = PhysicsShapeType::ConvexHull;
	hullBody.shape.points = {
		{ 0.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
	};
	hull.body = hullBody;

	document.world.push_back(sphereWorld);
	document.world.push_back(hullWorld);
	document.entities.push_back(sphere);
	document.entities.push_back(hull);
	return document;
}

[[nodiscard]] Result<void> RunSceneDocumentRoundTrip()
{
	const SceneDocument source = MakeRoundTripScene();
	const Result<std::string> written = WriteSceneDocument(source);
	const Result<SceneDocument> readBack = written
		? ReadSceneDocument(written.value)
		: MakeFail<SceneDocument>(written.error.code, written.error.message);
	const Result<std::string> writtenAgain = readBack
		? WriteSceneDocument(readBack.value)
		: MakeFail<std::string>(readBack.error.code, readBack.error.message);
	const Result<SceneDocument> readAgain = writtenAgain
		? ReadSceneDocument(writtenAgain.value)
		: MakeFail<SceneDocument>(writtenAgain.error.code, writtenAgain.error.message);
	const bool hullKept = readAgain && readAgain.value.entities.size() == 2
		&& readAgain.value.entities[1].body.has_value()
		&& readAgain.value.entities[1].body->shape.points.size() == 4
		&& readAgain.value.entities[1].mesh == "builtin:hull"
		&& RoundTripNearlyEqual(readAgain.value.entities[0].tint[0], 0.90f)
		&& RoundTripNearlyEqual(readAgain.value.world[0].transform.position.y, 4.0f)
		&& readAgain.value.entities[0].camera.has_value()
		&& RoundTripNearlyEqual(readAgain.value.entities[0].camera->fovYDegrees, 50.0f)
		&& RoundTripNearlyEqual(readAgain.value.entities[0].camera->nearPlane, 0.2f)
		&& RoundTripNearlyEqual(readAgain.value.entities[0].camera->farPlane, 500.0f)
		&& readAgain.value.entities[0].light.has_value()
		&& RoundTripNearlyEqual(readAgain.value.entities[0].light->intensity, 2.0f)
		&& RoundTripNearlyEqual(readAgain.value.entities[0].light->color[2], 0.3f)
		&& !readAgain.value.entities[1].camera.has_value()
		&& !readAgain.value.entities[1].light.has_value();
	if (!written || !readBack || !writtenAgain || !readAgain || !hullKept)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document JSON should round-trip mesh keys, tint, and owned hull points");
	}

	return MakeOk();
}
} // namespace

Result<void> RunDebugStartupTests(
	[[maybe_unused]] const std::filesystem::path& compiledShaderRoot,
	[[maybe_unused]] const std::filesystem::path& assetsRoot)
{
#ifdef _DEBUG
	if (auto result = RunShaderBytecodeLoaderTests(compiledShaderRoot); !result)
	{
		return result;
	}

	if (auto result = RunTextureFileLoaderTests(assetsRoot); !result)
	{
		return result;
	}

	if (auto result = RunFbxModelFileLoaderTests(assetsRoot); !result)
	{
		return result;
	}

	if (auto result = RunPrimitiveMeshTests(); !result)
	{
		return result;
	}

	if (auto result = RunWorldTests(); !result)
	{
		return result;
	}

	if (auto result = RunPhysicsLibraryTests(); !result)
	{
		return result;
	}

	if (auto result = RunPhysicsWorldTests(); !result)
	{
		return result;
	}

	if (auto result = RunEntityPhysicsTests(); !result)
	{
		return result;
	}

	if (auto result = RunWorldExtractTests(); !result)
	{
		return result;
	}

	if (auto result = RunSceneCaptureTests(); !result)
	{
		return result;
	}

	if (auto result = RunServiceLifetimeTests(); !result)
	{
		return result;
	}

	if (auto result = RunSceneDocumentLoadTests(assetsRoot); !result)
	{
		return result;
	}

	if (auto result = RunSceneDocumentRoundTrip(); !result)
	{
		return result;
	}

	if (auto result = RunRendererResourceSmokeLayoutTests(); !result)
	{
		return result;
	}

	if (auto result = RunFixedStepTests(); !result)
	{
		return result;
	}

	if (auto result = RunSubsystemOrderTests(); !result)
	{
		return result;
	}

	if (auto result = RunFrustumCullTests(); !result)
	{
		return result;
	}

	if (auto result = RunShadowMapTests(); !result)
	{
		return result;
	}
#endif

	return MakeOk();
}
