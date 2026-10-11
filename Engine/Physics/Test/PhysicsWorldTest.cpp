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

	PhysicsBoxDesc box{};
	box.halfExtent = { 0.5f, 0.25f, 1.0f };
	box.position = { 1.0f, 2.0f, 3.0f };
	box.rotation = rotation;

	Result<PhysicsBodyId> body = world.CreateBox(box);
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

	PhysicsBoxDesc box{};
	box.halfExtent = { 2.0f, 0.5f, 2.0f };
	box.position = { 0.0f, 0.5f, 0.0f };
	if (auto body = world.CreateBox(box); !body)
	{
		return FailInternal(LogCategory::Physics, body.error.code, body.error.message);
	}

	PhysicsSphereDesc sphere{};
	sphere.radius = 0.5f;
	sphere.position = { 0.0f, 4.0f, 0.0f };
	Result<PhysicsBodyId> sphereBody = world.CreateSphere(sphere);
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

	return TestSphereDrop();
}
