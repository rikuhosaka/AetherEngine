#pragma once

#include "Engine/Core/Handle/Handle.h"
#include "Engine/Core/Log/Result.h"

#include <DirectXMath.h>

#include <memory>
#include <span>

struct PhysicsBodyTag
{
};

using PhysicsBodyId = Handle<PhysicsBodyTag>;

struct PhysicsPose
{
	DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
};

enum class PhysicsMotion : uint8_t
{
	Static,
	Dynamic,
	Kinematic,
};

enum class PhysicsShapeType : uint8_t
{
	Box,
	Sphere,
	Capsule,
	ConvexHull,
	Mesh,
};

// Box reads halfExtent. Sphere reads radius. Capsule reads radius and halfHeight.
// ConvexHull reads points. Mesh reads points and indices. Extents are in meters.
// points and indices are only read during CreateBody.
struct PhysicsShape
{
	PhysicsShapeType type = PhysicsShapeType::Sphere;
	DirectX::XMFLOAT3 halfExtent{ 0.5f, 0.5f, 0.5f };
	float radius = 0.5f;
	float halfHeight = 0.5f;
	std::span<const DirectX::XMFLOAT3> points{};
	std::span<const uint32_t> indices{};
};

struct PhysicsRayHit
{
	PhysicsBodyId body{};
	float fraction = 0.0f;
	DirectX::XMFLOAT3 position{};
	DirectX::XMFLOAT3 normal{};
};

// normal pushes second out of first. Valid on began and on the matching end.
struct PhysicsContact
{
	PhysicsBodyId first{};
	PhysicsBodyId second{};
	DirectX::XMFLOAT3 point{};
	DirectX::XMFLOAT3 normal{};
};

// mass of 0 calculates mass from the shape density.
struct PhysicsBodyDesc
{
	PhysicsMotion motion = PhysicsMotion::Dynamic;
	PhysicsShape shape{};
	PhysicsPose pose{};
	float friction = 0.2f;
	float restitution = 0.0f;
	float mass = 0.0f;
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

	[[nodiscard]] Result<PhysicsBodyId> CreateBody(const PhysicsBodyDesc& desc);
	[[nodiscard]] Result<void> Destroy(PhysicsBodyId id);

	// Teleport. The next Sample uses this pose at every alpha.
	[[nodiscard]] Result<void> SetPose(PhysicsBodyId id, const PhysicsPose& pose);

	// Kinematic only. The next Step moves the body to target and pushes dynamic bodies.
	[[nodiscard]] Result<void> MoveKinematic(PhysicsBodyId id, const PhysicsPose& target, float deltaSeconds);

	// Dynamic only. Velocity is in engine space, meters per second.
	[[nodiscard]] Result<void> SetLinearVelocity(PhysicsBodyId id, const DirectX::XMFLOAT3& velocity);
	[[nodiscard]] Result<void> AddImpulse(PhysicsBodyId id, const DirectX::XMFLOAT3& impulse);

	// Engine space. The default is (0, -9.81, 0).
	[[nodiscard]] Result<void> SetGravity(const DirectX::XMFLOAT3& gravity);

	// One fixed simulation step. Stores the pose before and after the step for Sample.
	// Contact spans from the previous step are cleared at the start of a successful call.
	[[nodiscard]] Result<void> Step(float deltaSeconds);

	// Contacts opened or separated by the last successful Step. Sleep keeps a pair open.
	[[nodiscard]] std::span<const PhysicsContact> GetContactsBegan() const;
	[[nodiscard]] std::span<const PhysicsContact> GetContactsEnded() const;

	// Interpolates the poses from the last Step. alpha 0 is the pre-step pose.
	[[nodiscard]] bool Sample(PhysicsBodyId id, float alpha, PhysicsPose& outPose) const;

	[[nodiscard]] bool GetLinearVelocity(PhysicsBodyId id, DirectX::XMFLOAT3& outVelocity) const;

	// Closest hit in engine space. fraction 0 is the origin and 1 is origin + direction * maxDistance.
	// An invalid ignore body is not filtered.
	[[nodiscard]] bool RayCast(
		const DirectX::XMFLOAT3& origin,
		const DirectX::XMFLOAT3& direction,
		float maxDistance,
		PhysicsBodyId ignore,
		PhysicsRayHit& outHit) const;

private:
	struct Impl;

	explicit PhysicsWorld(std::unique_ptr<Impl> impl);

	std::unique_ptr<Impl> m_impl{};
};
