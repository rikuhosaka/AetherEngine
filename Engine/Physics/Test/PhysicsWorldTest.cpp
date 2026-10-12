#include "Engine/Physics/Test/PhysicsWorldTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Physics/PhysicsLibrary.h"
#include "Engine/Physics/PhysicsWorld.h"

#include <DirectXMath.h>

#include <cmath>
#include <format>

namespace
{
[[nodiscard]] bool IsNearlyEqual(float left, float right, float epsilon)
{
	return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] bool RotationsMatch(const DirectX::XMFLOAT4& left, const DirectX::XMFLOAT4& right)
{
	const float dot = std::fabs(DirectX::XMVectorGetX(DirectX::XMVector4Dot(
		DirectX::XMLoadFloat4(&left),
		DirectX::XMLoadFloat4(&right))));
	return IsNearlyEqual(dot, 1.0f, 0.001f);
}

[[nodiscard]] Result<void> TestPoseRoundTrip()
{
	Result<PhysicsWorld> created = PhysicsWorld::Create();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;
	DirectX::XMFLOAT4 rotation{};
	DirectX::XMStoreFloat4(
		&rotation,
		DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), DirectX::XM_PIDIV2));

	PhysicsBodyDesc box{};
	box.motion = PhysicsMotion::Static;
	box.shape.type = PhysicsShapeType::Box;
	box.shape.halfExtent = { 0.5f, 0.25f, 1.0f };
	box.pose.position = { 1.0f, 2.0f, 3.0f };
	box.pose.rotation = rotation;

	Result<PhysicsBodyId> body = world.CreateBody(box);
	if (!body)
	{
		return FailInternal(LogCategory::Physics, body.error.code, body.error.message);
	}

	if (auto step = world.Step(1.0f / 60.0f); !step)
	{
		return step;
	}

	PhysicsPose pose{};
	if (!world.Sample(body.value, 1.0f, pose))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"PhysicsWorld should sample a live static box");
	}

	if (!IsNearlyEqual(pose.position.x, 1.0f, 0.0001f)
		|| !IsNearlyEqual(pose.position.y, 2.0f, 0.0001f)
		|| !IsNearlyEqual(pose.position.z, 3.0f, 0.0001f)
		|| !RotationsMatch(pose.rotation, rotation))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Static box pose should survive a Jolt step in engine space");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestSphereDrop()
{
	Result<PhysicsWorld> created = PhysicsWorld::Create();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;

	PhysicsBodyDesc box{};
	box.motion = PhysicsMotion::Static;
	box.shape.type = PhysicsShapeType::Box;
	box.shape.halfExtent = { 2.0f, 0.5f, 2.0f };
	box.pose.position = { 0.0f, 0.5f, 0.0f };
	if (auto body = world.CreateBody(box); !body)
	{
		return FailInternal(LogCategory::Physics, body.error.code, body.error.message);
	}

	PhysicsBodyDesc sphere{};
	sphere.motion = PhysicsMotion::Dynamic;
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	sphere.pose.position = { 0.0f, 4.0f, 0.0f };
	Result<PhysicsBodyId> sphereBody = world.CreateBody(sphere);
	if (!sphereBody)
	{
		return FailInternal(LogCategory::Physics, sphereBody.error.code, sphereBody.error.message);
	}

	constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
	constexpr int kStepCount = 180;
	for (int stepIndex = 0; stepIndex < kStepCount; ++stepIndex)
	{
		if (auto step = world.Step(kFixedDeltaSeconds); !step)
		{
			return step;
		}
	}

	PhysicsPose pose{};
	if (!world.Sample(sphereBody.value, 1.0f, pose))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"PhysicsWorld should sample the dropped sphere");
	}

	DirectX::XMFLOAT3 velocity{};
	if (!world.GetLinearVelocity(sphereBody.value, velocity))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"PhysicsWorld should read the dropped sphere velocity");
	}

	// Block top is y = 1. Sphere radius is 0.5, so the resting center is y = 1.5.
	constexpr float kRestCenterY = 1.5f;
	if (!IsNearlyEqual(pose.position.y, kRestCenterY, 0.05f)
		|| !IsNearlyEqual(pose.position.x, 0.0f, 0.05f)
		|| !IsNearlyEqual(pose.position.z, 0.0f, 0.05f)
		|| !IsNearlyEqual(velocity.y, 0.0f, 0.1f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format(
				"Sphere should rest on the block, pose=({}, {}, {}) velocity.y={}",
				pose.position.x,
				pose.position.y,
				pose.position.z,
				velocity.y));
	}

	LOG_INFO_F(
		LogCategory::Physics,
		"Sphere drop rested at ({}, {}, {}), velocity.y={}",
		pose.position.x,
		pose.position.y,
		pose.position.z,
		velocity.y);
	return MakeOk();
}

[[nodiscard]] Result<PhysicsWorld> CreateTestWorld()
{
	return PhysicsWorld::Create();
}

[[nodiscard]] Result<void> StepCount(PhysicsWorld& world, int stepCount)
{
	constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
	for (int stepIndex = 0; stepIndex < stepCount; ++stepIndex)
	{
		if (auto step = world.Step(kFixedDeltaSeconds); !step)
		{
			return step;
		}
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestDynamicBoxOnStaticSphere()
{
	Result<PhysicsWorld> created = CreateTestWorld();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;

	PhysicsBodyDesc sphere{};
	sphere.motion = PhysicsMotion::Static;
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	sphere.pose.position = { 0.0f, 0.5f, 0.0f };
	if (auto body = world.CreateBody(sphere); !body)
	{
		return FailInternal(LogCategory::Physics, body.error.code, body.error.message);
	}

	PhysicsBodyDesc box{};
	box.motion = PhysicsMotion::Dynamic;
	box.shape.type = PhysicsShapeType::Box;
	box.shape.halfExtent = { 0.25f, 0.25f, 0.25f };
	box.pose.position = { 0.0f, 3.0f, 0.0f };
	box.friction = 1.0f;
	box.restitution = 0.0f;
	Result<PhysicsBodyId> boxBody = world.CreateBody(box);
	if (!boxBody)
	{
		return FailInternal(LogCategory::Physics, boxBody.error.code, boxBody.error.message);
	}

	if (auto stepped = StepCount(world, 180); !stepped)
	{
		return stepped;
	}

	PhysicsPose pose{};
	if (!world.Sample(boxBody.value, 1.0f, pose))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"PhysicsWorld should sample the dropped box");
	}

	DirectX::XMFLOAT3 velocity{};
	if (!world.GetLinearVelocity(boxBody.value, velocity))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"PhysicsWorld should read the dropped box velocity");
	}

	// Sphere top is y = 1. Box half height is 0.25, so the resting center is y = 1.25.
	constexpr float kRestCenterY = 1.25f;
	if (!IsNearlyEqual(pose.position.y, kRestCenterY, 0.08f)
		|| !IsNearlyEqual(pose.position.x, 0.0f, 0.15f)
		|| !IsNearlyEqual(pose.position.z, 0.0f, 0.15f)
		|| !IsNearlyEqual(velocity.y, 0.0f, 0.15f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format(
				"Dynamic box should rest on the static sphere, pose=({}, {}, {}) velocity.y={}",
				pose.position.x,
				pose.position.y,
				pose.position.z,
				velocity.y));
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestKinematicPose()
{
	Result<PhysicsWorld> created = CreateTestWorld();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;

	PhysicsBodyDesc box{};
	box.motion = PhysicsMotion::Kinematic;
	box.shape.type = PhysicsShapeType::Box;
	box.shape.halfExtent = { 0.5f, 0.5f, 0.5f };
	box.pose.position = { 0.0f, 3.0f, 0.0f };
	Result<PhysicsBodyId> body = world.CreateBody(box);
	if (!body)
	{
		return FailInternal(LogCategory::Physics, body.error.code, body.error.message);
	}

	if (auto stepped = StepCount(world, 60); !stepped)
	{
		return stepped;
	}

	PhysicsPose stayed{};
	if (!world.Sample(body.value, 1.0f, stayed)
		|| !IsNearlyEqual(stayed.position.y, 3.0f, 0.0001f))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Kinematic body should stay in place across steps");
	}

	DirectX::XMFLOAT4 rotation{};
	DirectX::XMStoreFloat4(
		&rotation,
		DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), DirectX::XM_PIDIV2));
	PhysicsPose teleported{};
	teleported.position = { 1.0f, 2.0f, -3.0f };
	teleported.rotation = rotation;
	if (auto moved = world.SetPose(body.value, teleported); !moved)
	{
		return moved;
	}

	PhysicsPose sampledAtZero{};
	PhysicsPose sampledAtOne{};
	if (!world.Sample(body.value, 0.0f, sampledAtZero) || !world.Sample(body.value, 1.0f, sampledAtOne))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"PhysicsWorld should sample a teleported kinematic body");
	}

	if (!IsNearlyEqual(sampledAtZero.position.x, 1.0f, 0.0001f)
		|| !IsNearlyEqual(sampledAtZero.position.y, 2.0f, 0.0001f)
		|| !IsNearlyEqual(sampledAtZero.position.z, -3.0f, 0.0001f)
		|| !IsNearlyEqual(sampledAtOne.position.z, -3.0f, 0.0001f)
		|| !RotationsMatch(sampledAtZero.rotation, rotation)
		|| !RotationsMatch(sampledAtOne.rotation, rotation))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"SetPose should replace both interpolation endpoints");
	}

	constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
	PhysicsPose target = teleported;
	target.position.y = 4.0f;
	if (auto kinematic = world.MoveKinematic(body.value, target, kFixedDeltaSeconds); !kinematic)
	{
		return kinematic;
	}

	if (auto step = world.Step(kFixedDeltaSeconds); !step)
	{
		return step;
	}

	PhysicsPose moved{};
	if (!world.Sample(body.value, 1.0f, moved)
		|| !IsNearlyEqual(moved.position.x, 1.0f, 0.02f)
		|| !IsNearlyEqual(moved.position.y, 4.0f, 0.02f)
		|| !IsNearlyEqual(moved.position.z, -3.0f, 0.02f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format(
				"MoveKinematic should reach the target, pose=({}, {}, {})",
				moved.position.x,
				moved.position.y,
				moved.position.z));
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestDynamicWrites()
{
	Result<PhysicsWorld> created = CreateTestWorld();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;

	PhysicsBodyDesc sphere{};
	sphere.motion = PhysicsMotion::Dynamic;
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	sphere.mass = 2.0f;
	sphere.pose.position = { 0.0f, 5.0f, 0.0f };
	Result<PhysicsBodyId> body = world.CreateBody(sphere);
	if (!body)
	{
		return FailInternal(LogCategory::Physics, body.error.code, body.error.message);
	}

	const DirectX::XMFLOAT3 velocity{ 1.0f, 2.0f, 3.0f };
	if (auto written = world.SetLinearVelocity(body.value, velocity); !written)
	{
		return written;
	}

	DirectX::XMFLOAT3 readVelocity{};
	if (!world.GetLinearVelocity(body.value, readVelocity)
		|| !IsNearlyEqual(readVelocity.x, 1.0f, 0.0001f)
		|| !IsNearlyEqual(readVelocity.y, 2.0f, 0.0001f)
		|| !IsNearlyEqual(readVelocity.z, 3.0f, 0.0001f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format(
				"Linear velocity should round-trip in engine space, velocity=({}, {}, {})",
				readVelocity.x,
				readVelocity.y,
				readVelocity.z));
	}

	if (auto impulse = world.AddImpulse(body.value, { 0.0f, 0.0f, 2.0f }); !impulse)
	{
		return impulse;
	}

	if (!world.GetLinearVelocity(body.value, readVelocity)
		|| !IsNearlyEqual(readVelocity.z, 4.0f, 0.02f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format("Impulse should change linear velocity, velocity.z={}", readVelocity.z));
	}

	PhysicsBodyDesc staticBox{};
	staticBox.motion = PhysicsMotion::Static;
	staticBox.shape.type = PhysicsShapeType::Box;
	staticBox.shape.halfExtent = { 0.5f, 0.5f, 0.5f };
	Result<PhysicsBodyId> staticBody = world.CreateBody(staticBox);
	if (!staticBody)
	{
		return FailInternal(LogCategory::Physics, staticBody.error.code, staticBody.error.message);
	}

	if (world.SetLinearVelocity(staticBody.value, velocity))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"SetLinearVelocity should reject a static body");
	}

	if (world.AddImpulse(staticBody.value, velocity))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"AddImpulse should reject a static body");
	}

	if (world.MoveKinematic(body.value, PhysicsPose{}, 1.0f / 60.0f))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"MoveKinematic should reject a dynamic body");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestDestroyReusesGeneration()
{
	Result<PhysicsWorld> created = CreateTestWorld();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;

	PhysicsBodyDesc sphere{};
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	Result<PhysicsBodyId> first = world.CreateBody(sphere);
	if (!first)
	{
		return FailInternal(LogCategory::Physics, first.error.code, first.error.message);
	}

	if (auto destroyed = world.Destroy(first.value); !destroyed)
	{
		return destroyed;
	}

	PhysicsPose pose{};
	if (world.Sample(first.value, 1.0f, pose))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Sample should fail for a destroyed body");
	}

	Result<PhysicsBodyId> second = world.CreateBody(sphere);
	if (!second)
	{
		return FailInternal(LogCategory::Physics, second.error.code, second.error.message);
	}

	if (second.value.Index != first.value.Index || second.value.Generation == first.value.Generation)
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Recreated body should reuse the index with a new generation");
	}

	if (world.Sample(first.value, 1.0f, pose) || !world.Sample(second.value, 1.0f, pose))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Only the current generation should sample");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestInvalidBodyDesc()
{
	Result<PhysicsWorld> created = CreateTestWorld();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;
	PhysicsWorld empty{};

	PhysicsBodyDesc sphere{};
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	if (empty.CreateBody(sphere))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"CreateBody should reject a default world");
	}

	PhysicsBodyDesc flatBox{};
	flatBox.shape.type = PhysicsShapeType::Box;
	flatBox.shape.halfExtent = { 0.5f, 0.0f, 0.5f };
	PhysicsBodyDesc zeroRadius{};
	zeroRadius.shape.type = PhysicsShapeType::Sphere;
	zeroRadius.shape.radius = 0.0f;
	PhysicsBodyDesc negativeFriction = sphere;
	negativeFriction.friction = -0.1f;
	PhysicsBodyDesc negativeMass = sphere;
	negativeMass.mass = -1.0f;

	if (world.CreateBody(flatBox) || world.CreateBody(zeroRadius) || world.CreateBody(negativeFriction) || world.CreateBody(negativeMass))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"CreateBody should reject invalid shape, friction, and mass");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestZeroGravity()
{
	Result<PhysicsWorld> created = CreateTestWorld();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;
	if (auto gravity = world.SetGravity({ 0.0f, 0.0f, 0.0f }); !gravity)
	{
		return gravity;
	}

	PhysicsBodyDesc sphere{};
	sphere.motion = PhysicsMotion::Dynamic;
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	sphere.pose.position = { 0.0f, 4.0f, 0.0f };
	Result<PhysicsBodyId> body = world.CreateBody(sphere);
	if (!body)
	{
		return FailInternal(LogCategory::Physics, body.error.code, body.error.message);
	}

	if (auto step = world.Step(1.0f / 60.0f); !step)
	{
		return step;
	}

	PhysicsPose pose{};
	DirectX::XMFLOAT3 velocity{};
	if (!world.Sample(body.value, 1.0f, pose) || !world.GetLinearVelocity(body.value, velocity))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"PhysicsWorld should sample a body under zero gravity");
	}

	if (!IsNearlyEqual(pose.position.y, 4.0f, 0.0001f) || !IsNearlyEqual(velocity.y, 0.0f, 0.0001f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format(
				"Zero gravity should leave the sphere at rest, pose.y={} velocity.y={}",
				pose.position.y,
				velocity.y));
	}

	return MakeOk();
}

[[nodiscard]] bool IsContactPair(const PhysicsContact& contact, PhysicsBodyId first, PhysicsBodyId second)
{
	return (contact.first == first && contact.second == second)
		|| (contact.first == second && contact.second == first);
}

[[nodiscard]] Result<void> TestRayCast()
{
	PhysicsWorld empty{};
	PhysicsRayHit ignoredHit{};
	if (empty.RayCast({ 0.0f, 3.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 5.0f, {}, ignoredHit))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"RayCast should miss on a default world");
	}

	Result<PhysicsWorld> created = CreateTestWorld();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;
	PhysicsBodyDesc floor{};
	floor.motion = PhysicsMotion::Static;
	floor.shape.type = PhysicsShapeType::Box;
	floor.shape.halfExtent = { 2.0f, 0.5f, 2.0f };
	floor.pose.position = { 0.0f, 0.5f, 0.0f };
	Result<PhysicsBodyId> floorBody = world.CreateBody(floor);
	if (!floorBody)
	{
		return FailInternal(LogCategory::Physics, floorBody.error.code, floorBody.error.message);
	}

	PhysicsRayHit downHit{};
	if (!world.RayCast({ 0.0f, 3.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 5.0f, {}, downHit))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"RayCast should hit the static box from above");
	}

	if (downHit.body != floorBody.value
		|| !IsNearlyEqual(downHit.fraction, 0.4f, 0.02f)
		|| !IsNearlyEqual(downHit.position.y, 1.0f, 0.02f)
		|| !IsNearlyEqual(downHit.normal.x, 0.0f, 0.05f)
		|| !IsNearlyEqual(downHit.normal.y, 1.0f, 0.05f)
		|| !IsNearlyEqual(downHit.normal.z, 0.0f, 0.05f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format(
				"Down ray should hit the box top, fraction={} position.y={} normal=({}, {}, {})",
				downHit.fraction,
				downHit.position.y,
				downHit.normal.x,
				downHit.normal.y,
				downHit.normal.z));
	}

	PhysicsRayHit ignoredDown{};
	if (world.RayCast({ 0.0f, 3.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 5.0f, floorBody.value, ignoredDown))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"RayCast should miss when the only body is ignored");
	}

	PhysicsBodyDesc wall{};
	wall.motion = PhysicsMotion::Static;
	wall.shape.type = PhysicsShapeType::Box;
	wall.shape.halfExtent = { 0.5f, 0.5f, 0.5f };
	wall.pose.position = { 0.0f, 0.5f, 2.0f };
	Result<PhysicsBodyId> wallBody = world.CreateBody(wall);
	if (!wallBody)
	{
		return FailInternal(LogCategory::Physics, wallBody.error.code, wallBody.error.message);
	}

	PhysicsRayHit forwardHit{};
	if (!world.RayCast({ 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, 5.0f, floorBody.value, forwardHit))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"RayCast along +Z should hit the wall");
	}

	if (forwardHit.body != wallBody.value
		|| !IsNearlyEqual(forwardHit.position.z, 1.5f, 0.02f)
		|| !(forwardHit.position.z > 0.0f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format("Forward ray hit should stay in positive engine Z, position.z={}", forwardHit.position.z));
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestContactBeginSleepAndEnd()
{
	Result<PhysicsWorld> created = CreateTestWorld();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;

	PhysicsBodyDesc floor{};
	floor.motion = PhysicsMotion::Static;
	floor.shape.type = PhysicsShapeType::Box;
	floor.shape.halfExtent = { 2.0f, 0.5f, 2.0f };
	floor.pose.position = { 0.0f, 0.5f, 0.0f };
	Result<PhysicsBodyId> floorBody = world.CreateBody(floor);
	if (!floorBody)
	{
		return FailInternal(LogCategory::Physics, floorBody.error.code, floorBody.error.message);
	}

	PhysicsBodyDesc sphere{};
	sphere.motion = PhysicsMotion::Dynamic;
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	sphere.restitution = 0.0f;
	sphere.pose.position = { 0.0f, 4.0f, 0.0f };
	Result<PhysicsBodyId> sphereBody = world.CreateBody(sphere);
	if (!sphereBody)
	{
		return FailInternal(LogCategory::Physics, sphereBody.error.code, sphereBody.error.message);
	}

	int beginCount = 0;
	int endCount = 0;
	PhysicsContact beganContact{};
	constexpr int kSettleSteps = 240;
	for (int stepIndex = 0; stepIndex < kSettleSteps; ++stepIndex)
	{
		if (auto step = world.Step(1.0f / 60.0f); !step)
		{
			return step;
		}

		const std::span<const PhysicsContact> began = world.GetContactsBegan();
		const std::span<const PhysicsContact> ended = world.GetContactsEnded();
		if (began.size() > 1 || ended.size() > 1)
		{
			return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
				"A single pair should produce at most one contact event per step");
		}

		if (!began.empty())
		{
			beganContact = began[0];
		}

		beginCount += static_cast<int>(began.size());
		endCount += static_cast<int>(ended.size());
	}

	if (beginCount != 1
		|| endCount != 0
		|| !IsContactPair(beganContact, floorBody.value, sphereBody.value))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format(
				"The dropped sphere should begin contact once and stay through sleep, begins={} ends={}",
				beginCount,
				endCount));
	}

	PhysicsPose separated{};
	separated.position = { 0.0f, 8.0f, 0.0f };
	if (auto moved = world.SetPose(sphereBody.value, separated); !moved)
	{
		return moved;
	}

	if (auto step = world.Step(1.0f / 60.0f); !step)
	{
		return step;
	}

	const std::span<const PhysicsContact> ended = world.GetContactsEnded();
	if (ended.size() != 1 || !IsContactPair(ended[0], floorBody.value, sphereBody.value) || !world.GetContactsBegan().empty())
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format("Separating the active sphere should end the contact once, ends={}", ended.size()));
	}

	if (auto step = world.Step(1.0f / 60.0f); !step)
	{
		return step;
	}

	if (!world.GetContactsBegan().empty() || !world.GetContactsEnded().empty())
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"The separated pair should not emit another contact event");
	}

	return MakeOk();
}

[[nodiscard]] PhysicsBodyDesc MakeStaticBox(DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 halfExtent)
{
	PhysicsBodyDesc desc{};
	desc.motion = PhysicsMotion::Static;
	desc.shape.type = PhysicsShapeType::Box;
	desc.shape.halfExtent = halfExtent;
	desc.pose.position = position;
	return desc;
}

[[nodiscard]] Result<void> TestCapsuleDropAndRay()
{
	Result<PhysicsWorld> created = CreateTestWorld();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;
	if (auto floor = world.CreateBody(MakeStaticBox({ 0.0f, 0.5f, 0.0f }, { 2.0f, 0.5f, 2.0f })); !floor)
	{
		return FailInternal(LogCategory::Physics, floor.error.code, floor.error.message);
	}

	PhysicsBodyDesc capsule{};
	capsule.motion = PhysicsMotion::Dynamic;
	capsule.shape.type = PhysicsShapeType::Capsule;
	capsule.shape.radius = 0.25f;
	capsule.shape.halfHeight = 0.5f;
	capsule.restitution = 0.0f;
	capsule.pose.position = { 0.0f, 4.0f, 0.0f };
	Result<PhysicsBodyId> capsuleBody = world.CreateBody(capsule);
	if (!capsuleBody)
	{
		return FailInternal(LogCategory::Physics, capsuleBody.error.code, capsuleBody.error.message);
	}

	if (auto stepped = StepCount(world, 180); !stepped)
	{
		return stepped;
	}

	PhysicsPose pose{};
	DirectX::XMFLOAT3 velocity{};
	if (!world.Sample(capsuleBody.value, 1.0f, pose) || !world.GetLinearVelocity(capsuleBody.value, velocity))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"PhysicsWorld should sample the dropped capsule");
	}

	constexpr float kRestCenterY = 1.75f;
	if (!IsNearlyEqual(pose.position.y, kRestCenterY, 0.08f)
		|| !IsNearlyEqual(pose.position.x, 0.0f, 0.08f)
		|| !IsNearlyEqual(pose.position.z, 0.0f, 0.08f)
		|| !IsNearlyEqual(velocity.y, 0.0f, 0.15f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format(
				"Capsule should rest on the box, pose=({}, {}, {}) velocity.y={}",
				pose.position.x,
				pose.position.y,
				pose.position.z,
				velocity.y));
	}

	PhysicsBodyDesc upright{};
	upright.motion = PhysicsMotion::Static;
	upright.shape.type = PhysicsShapeType::Capsule;
	upright.shape.radius = 0.25f;
	upright.shape.halfHeight = 0.5f;
	upright.pose.position = { 5.0f, 2.0f, 0.0f };
	Result<PhysicsBodyId> uprightBody = world.CreateBody(upright);
	if (!uprightBody)
	{
		return FailInternal(LogCategory::Physics, uprightBody.error.code, uprightBody.error.message);
	}

	PhysicsRayHit hit{};
	if (!world.RayCast({ 5.0f, 4.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 5.0f, {}, hit)
		|| hit.body != uprightBody.value
		|| !IsNearlyEqual(hit.fraction, 0.25f, 0.02f)
		|| !IsNearlyEqual(hit.position.y, 2.75f, 0.02f)
		|| !IsNearlyEqual(hit.normal.y, 1.0f, 0.05f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format(
				"Ray should hit the capsule top, fraction={} position.y={} normal.y={}",
				hit.fraction,
				hit.position.y,
				hit.normal.y));
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestConvexHullDrop()
{
	Result<PhysicsWorld> created = CreateTestWorld();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;
	if (auto floor = world.CreateBody(MakeStaticBox({ 0.0f, 0.5f, 0.0f }, { 2.0f, 0.5f, 2.0f })); !floor)
	{
		return FailInternal(LogCategory::Physics, floor.error.code, floor.error.message);
	}

	constexpr float kHalf = 0.25f;
	const std::array<DirectX::XMFLOAT3, 8> corners = {{
		{ -kHalf, -kHalf, -kHalf },
		{ kHalf, -kHalf, -kHalf },
		{ kHalf, kHalf, -kHalf },
		{ -kHalf, kHalf, -kHalf },
		{ -kHalf, -kHalf, kHalf },
		{ kHalf, -kHalf, kHalf },
		{ kHalf, kHalf, kHalf },
		{ -kHalf, kHalf, kHalf },
	}};

	PhysicsBodyDesc hull{};
	hull.motion = PhysicsMotion::Dynamic;
	hull.shape.type = PhysicsShapeType::ConvexHull;
	hull.shape.points = corners;
	hull.restitution = 0.0f;
	hull.pose.position = { 0.0f, 3.0f, 0.0f };
	Result<PhysicsBodyId> hullBody = world.CreateBody(hull);
	if (!hullBody)
	{
		return FailInternal(LogCategory::Physics, hullBody.error.code, hullBody.error.message);
	}

	if (auto stepped = StepCount(world, 180); !stepped)
	{
		return stepped;
	}

	PhysicsPose pose{};
	DirectX::XMFLOAT3 velocity{};
	if (!world.Sample(hullBody.value, 1.0f, pose) || !world.GetLinearVelocity(hullBody.value, velocity))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"PhysicsWorld should sample the dropped convex hull");
	}

	constexpr float kRestCenterY = 1.25f;
	if (!IsNearlyEqual(pose.position.y, kRestCenterY, 0.08f)
		|| !IsNearlyEqual(pose.position.x, 0.0f, 0.08f)
		|| !IsNearlyEqual(pose.position.z, 0.0f, 0.08f)
		|| !IsNearlyEqual(velocity.y, 0.0f, 0.15f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format(
				"Convex hull should rest on the box, pose=({}, {}, {}) velocity.y={}",
				pose.position.x,
				pose.position.y,
				pose.position.z,
				velocity.y));
	}

	const std::array<DirectX::XMFLOAT3, 3> flat = {{
		{ 0.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
	}};
	PhysicsBodyDesc invalid{};
	invalid.shape.type = PhysicsShapeType::ConvexHull;
	invalid.shape.points = flat;
	if (world.CreateBody(invalid))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"CreateBody should reject a convex hull with fewer than four points");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestStaticMeshFloor()
{
	Result<PhysicsWorld> created = CreateTestWorld();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;
	const std::array<DirectX::XMFLOAT3, 4> corners = {{
		{ -2.0f, 0.0f, -2.0f },
		{ -2.0f, 0.0f, 2.0f },
		{ 2.0f, 0.0f, -2.0f },
		{ 2.0f, 0.0f, 2.0f },
	}};
	const std::array<uint32_t, 6> indices = { 0, 1, 2, 3, 2, 1 };

	PhysicsBodyDesc floor{};
	floor.motion = PhysicsMotion::Static;
	floor.shape.type = PhysicsShapeType::Mesh;
	floor.shape.points = corners;
	floor.shape.indices = indices;
	Result<PhysicsBodyId> floorBody = world.CreateBody(floor);
	if (!floorBody)
	{
		return FailInternal(LogCategory::Physics, floorBody.error.code, floorBody.error.message);
	}

	PhysicsBodyDesc sphere{};
	sphere.motion = PhysicsMotion::Dynamic;
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	sphere.restitution = 0.0f;
	sphere.pose.position = { 0.0f, 1.0f, 0.0f };
	Result<PhysicsBodyId> sphereBody = world.CreateBody(sphere);
	if (!sphereBody)
	{
		return FailInternal(LogCategory::Physics, sphereBody.error.code, sphereBody.error.message);
	}

	if (auto stepped = StepCount(world, 120); !stepped)
	{
		return stepped;
	}

	PhysicsPose pose{};
	DirectX::XMFLOAT3 velocity{};
	if (!world.Sample(sphereBody.value, 1.0f, pose) || !world.GetLinearVelocity(sphereBody.value, velocity))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"PhysicsWorld should sample the sphere on the mesh");
	}

	if (!IsNearlyEqual(pose.position.y, 0.5f, 0.08f)
		|| !IsNearlyEqual(pose.position.x, 0.0f, 0.08f)
		|| !IsNearlyEqual(pose.position.z, 0.0f, 0.08f)
		|| !IsNearlyEqual(velocity.y, 0.0f, 0.15f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format(
				"Sphere should rest on the mesh, pose=({}, {}, {}) velocity.y={}",
				pose.position.x,
				pose.position.y,
				pose.position.z,
				velocity.y));
	}

	PhysicsRayHit hit{};
	if (!world.RayCast({ 0.0f, 3.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 5.0f, sphereBody.value, hit)
		|| hit.body != floorBody.value
		|| !IsNearlyEqual(hit.position.y, 0.0f, 0.02f)
		|| !(hit.normal.y > 0.5f))
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format(
				"Ray should hit the mesh floor, position.y={} normal.y={}",
				hit.position.y,
				hit.normal.y));
	}

	PhysicsBodyDesc dynamicMesh = floor;
	dynamicMesh.motion = PhysicsMotion::Dynamic;
	if (world.CreateBody(dynamicMesh))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"CreateBody should reject a dynamic mesh");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestInvalidShapes()
{
	Result<PhysicsWorld> created = CreateTestWorld();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsWorld& world = created.value;

	PhysicsBodyDesc zeroRadius{};
	zeroRadius.shape.type = PhysicsShapeType::Capsule;
	zeroRadius.shape.radius = 0.0f;
	zeroRadius.shape.halfHeight = 0.5f;

	PhysicsBodyDesc zeroHeight{};
	zeroHeight.shape.type = PhysicsShapeType::Capsule;
	zeroHeight.shape.radius = 0.25f;
	zeroHeight.shape.halfHeight = 0.0f;

	const std::array<DirectX::XMFLOAT3, 3> corners = {{
		{ 0.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
	}};
	const std::array<uint32_t, 2> shortIndices = { 0, 1 };
	const std::array<uint32_t, 3> missingIndex = { 0, 1, 9 };
	const std::array<uint32_t, 3> degenerate = { 0, 1, 2 };
	const std::array<DirectX::XMFLOAT3, 3> line = {{
		{ 0.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 2.0f, 0.0f, 0.0f },
	}};

	PhysicsBodyDesc shortMesh{};
	shortMesh.motion = PhysicsMotion::Static;
	shortMesh.shape.type = PhysicsShapeType::Mesh;
	shortMesh.shape.points = corners;
	shortMesh.shape.indices = shortIndices;

	PhysicsBodyDesc missingMesh{};
	missingMesh.motion = PhysicsMotion::Static;
	missingMesh.shape.type = PhysicsShapeType::Mesh;
	missingMesh.shape.points = corners;
	missingMesh.shape.indices = missingIndex;

	PhysicsBodyDesc degenerateMesh{};
	degenerateMesh.motion = PhysicsMotion::Static;
	degenerateMesh.shape.type = PhysicsShapeType::Mesh;
	degenerateMesh.shape.points = line;
	degenerateMesh.shape.indices = degenerate;

	if (world.CreateBody(zeroRadius)
		|| world.CreateBody(zeroHeight)
		|| world.CreateBody(shortMesh)
		|| world.CreateBody(missingMesh)
		|| world.CreateBody(degenerateMesh))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"CreateBody should reject invalid capsule and mesh shapes");
	}

	return MakeOk();
}
} // namespace

Result<void> RunPhysicsWorldTests()
{
	if (auto library = InitializePhysicsLibrary(); !library)
	{
		return library;
	}

	if (auto roundTrip = TestPoseRoundTrip(); !roundTrip)
	{
		return roundTrip;
	}

	if (auto drop = TestSphereDrop(); !drop)
	{
		return drop;
	}

	if (auto boxOnSphere = TestDynamicBoxOnStaticSphere(); !boxOnSphere)
	{
		return boxOnSphere;
	}

	if (auto kinematic = TestKinematicPose(); !kinematic)
	{
		return kinematic;
	}

	if (auto writes = TestDynamicWrites(); !writes)
	{
		return writes;
	}

	if (auto destroyed = TestDestroyReusesGeneration(); !destroyed)
	{
		return destroyed;
	}

	if (auto invalid = TestInvalidBodyDesc(); !invalid)
	{
		return invalid;
	}

	if (auto gravity = TestZeroGravity(); !gravity)
	{
		return gravity;
	}

	if (auto ray = TestRayCast(); !ray)
	{
		return ray;
	}

	if (auto contacts = TestContactBeginSleepAndEnd(); !contacts)
	{
		return contacts;
	}

	if (auto capsule = TestCapsuleDropAndRay(); !capsule)
	{
		return capsule;
	}

	if (auto hull = TestConvexHullDrop(); !hull)
	{
		return hull;
	}

	if (auto mesh = TestStaticMeshFloor(); !mesh)
	{
		return mesh;
	}

	return TestInvalidShapes();
}
