#include "Engine/Scene/SceneCaptureTest.h"

#include "Engine/Scene/SceneCapture.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/World/World.h"

#include <cmath>
#include <string>
#include <string_view>

namespace
{
[[nodiscard]] bool IsNearlyEqual(float left, float right, float epsilon = 0.0001f)
{
	return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] bool SamePoint(const DirectX::XMFLOAT3& left, const DirectX::XMFLOAT3& right)
{
	return IsNearlyEqual(left.x, right.x) && IsNearlyEqual(left.y, right.y) && IsNearlyEqual(left.z, right.z);
}

[[nodiscard]] SceneDocument MakeScene()
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

	WorldDocumentEntity hullWorld{};
	hullWorld.id = 2;
	hullWorld.name = "Hull";
	hullWorld.transform.position = { 1.0f, 3.0f, -0.5f };
	SceneEntity hull{};
	hull.id = 2;
	hull.mesh = "builtin:hull";
	hull.tint[0] = 0.30f;
	hull.tint[1] = 0.72f;
	hull.tint[2] = 0.38f;
	hull.tint[3] = 1.0f;
	RigidBody hullBody{};
	hullBody.motion = PhysicsMotion::Dynamic;
	hullBody.friction = 0.4f;
	hullBody.shape.type = PhysicsShapeType::ConvexHull;
	hullBody.shape.points = {
		{ 0.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
	};
	hull.body = hullBody;

	WorldDocumentEntity markerWorld{};
	markerWorld.id = 3;
	markerWorld.name = "Marker";
	markerWorld.visible = false;
	SceneEntity marker{};
	marker.id = 3;
	marker.mesh = "builtin:box";
	marker.tint[0] = 0.2f;
	marker.tint[1] = 0.2f;
	marker.tint[2] = 0.2f;
	marker.tint[3] = 1.0f;

	document.world.push_back(sphereWorld);
	document.world.push_back(hullWorld);
	document.world.push_back(markerWorld);
	document.entities.push_back(sphere);
	document.entities.push_back(hull);
	document.entities.push_back(marker);
	return document;
}

[[nodiscard]] const SceneEntity* FindNamedEntity(const SceneDocument& document, std::string_view name)
{
	for (size_t index = 0; index < document.world.size() && index < document.entities.size(); ++index)
	{
		if (document.world[index].name == name)
		{
			return &document.entities[index];
		}
	}

	return nullptr;
}

[[nodiscard]] const WorldDocumentEntity* FindNamedWorld(const SceneDocument& document, std::string_view name)
{
	for (const WorldDocumentEntity& entity : document.world)
	{
		if (entity.name == name)
		{
			return &entity;
		}
	}

	return nullptr;
}

[[nodiscard]] Result<void> TestCaptureScene()
{
	const SceneDocument source = MakeScene();
	World world{};

	WorldSpawnDesc sphereDesc{};
	sphereDesc.name = "Sphere";
	sphereDesc.transform = source.world[0].transform;
	const EntityId sphere = world.Spawn(std::move(sphereDesc));

	WorldSpawnDesc hullDesc{};
	hullDesc.name = "Hull";
	hullDesc.transform = source.world[1].transform;
	const EntityId hull = world.Spawn(std::move(hullDesc));

	WorldSpawnDesc markerDesc{};
	markerDesc.name = "Marker";
	const EntityId marker = world.Spawn(std::move(markerDesc));
	Renderable markerRenderable{};
	markerRenderable.visible = false;
	if (!world.Add<Renderable>(sphere, Renderable{}) || !world.Add<Renderable>(hull, Renderable{})
		|| !world.Add<Renderable>(marker, std::move(markerRenderable))
		|| !source.entities[0].body.has_value() || !world.Add<RigidBody>(sphere, *source.entities[0].body)
		|| !source.entities[1].body.has_value() || !world.Add<RigidBody>(hull, *source.entities[1].body))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene capture test should add renderables and rigid bodies");
	}
	Transform* sphereTransform = world.GetTransform(sphere);
	if (!sphere.IsValid() || !hull.IsValid() || !marker.IsValid() || sphereTransform == nullptr
		|| !world.SetParent(marker, sphere))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene capture test should spawn and reparent the marker");
	}

	sphereTransform->position = { 9.0f, 8.0f, 7.0f };
	const WorldDocumentBinding bindings[] = {
		{ 1, sphere },
		{ 2, hull },
		{ 3, marker },
	};
	const Result<SceneDocument> captured = CaptureScene(world, source, bindings);
	const WorldDocumentEntity* capturedSphere = captured ? FindNamedWorld(captured.value, "Sphere") : nullptr;
	const WorldDocumentEntity* capturedMarker = captured ? FindNamedWorld(captured.value, "Marker") : nullptr;
	const SceneEntity* capturedHull = captured ? FindNamedEntity(captured.value, "Hull") : nullptr;
	const bool hullKept = capturedHull != nullptr && capturedHull->mesh == "builtin:hull" && capturedHull->body.has_value()
		&& capturedHull->body->shape.points.size() == source.entities[1].body->shape.points.size()
		&& SamePoint(capturedHull->body->shape.points[0], source.entities[1].body->shape.points[0]);
	if (!captured || capturedSphere == nullptr || capturedMarker == nullptr || !hullKept
		|| !IsNearlyEqual(capturedSphere->transform.position.x, 9.0f)
		|| !IsNearlyEqual(capturedSphere->transform.position.y, 8.0f)
		|| !IsNearlyEqual(capturedSphere->transform.position.z, 7.0f)
		|| capturedMarker->parent != 1)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene capture should update the local transform and parent without changing the hull");
	}

	world.Destroy(sphere);
	if (CaptureScene(world, source, bindings))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene capture should fail when an editor entity is gone");
	}

	return MakeOk();
}
} // namespace

Result<void> RunSceneCaptureTests()
{
	if (auto captured = TestCaptureScene(); !captured)
	{
		return captured;
	}

	LOG_INFO(LogCategory::ECS, "SceneCaptureTests passed");
	return MakeOk();
}
