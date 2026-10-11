#pragma once

#include "Engine/Core/Handle/Handle.h"
#include "Engine/Core/Log/Result.h"

#include <DirectXMath.h>

#include <memory>

struct PhysicsBodyTag
{
};

using PhysicsBodyId = Handle<PhysicsBodyTag>;

struct PhysicsPose
{
	DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
};

// Static box. Half extents are in meters, centered on position.
struct PhysicsBoxDesc
{
	DirectX::XMFLOAT3 halfExtent{ 0.5f, 0.5f, 0.5f };
	DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
};

// Dynamic sphere. Radius is in meters, centered on position.
struct PhysicsSphereDesc
{
	float radius = 0.5f;
	DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
};

// One Jolt physics system. InitializePhysicsLibrary must succeed before Create.
// Poses are in the engine's left-handed Y-up space.
class PhysicsWorld
{
public:
	PhysicsWorld();
	~PhysicsWorld();

	PhysicsWorld(PhysicsWorld&& other) noexcept;
	PhysicsWorld& operator=(PhysicsWorld&& other) noexcept;

	PhysicsWorld(const PhysicsWorld&) = delete;
	PhysicsWorld& operator=(const PhysicsWorld&) = delete;

	[[nodiscard]] static Result<PhysicsWorld> Create();

	[[nodiscard]] Result<PhysicsBodyId> CreateBox(const PhysicsBoxDesc& desc);
	[[nodiscard]] Result<PhysicsBodyId> CreateSphere(const PhysicsSphereDesc& desc);

	// One fixed simulation step. Stores the pose before and after the step for Sample.
	[[nodiscard]] Result<void> Step(float deltaSeconds);

	// Interpolates the poses from the last Step. alpha 0 is the pre-step pose.
	[[nodiscard]] bool Sample(PhysicsBodyId id, float alpha, PhysicsPose& outPose) const;

	[[nodiscard]] bool GetLinearVelocity(PhysicsBodyId id, DirectX::XMFLOAT3& outVelocity) const;

private:
	struct Impl;

	explicit PhysicsWorld(std::unique_ptr<Impl> impl);

	std::unique_ptr<Impl> m_impl{};
};
