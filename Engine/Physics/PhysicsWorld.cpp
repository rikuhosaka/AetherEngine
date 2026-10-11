#include "Engine/Physics/PhysicsWorld.h"

#include "Engine/Core/Log/Result.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <utility>
#include <vector>

namespace
{
namespace ObjectLayers
{
constexpr JPH::ObjectLayer Static = 0;
constexpr JPH::ObjectLayer Dynamic = 1;
constexpr JPH::ObjectLayer Count = 2;
} // namespace ObjectLayers

namespace BroadPhaseLayers
{
constexpr JPH::BroadPhaseLayer::Type NonMoving = 0;
constexpr JPH::BroadPhaseLayer::Type Moving = 1;
constexpr uint32_t Count = 2;
} // namespace BroadPhaseLayers

constexpr uint32_t kMaxBodies = 1024;
constexpr uint32_t kMaxBodyPairs = 1024;
constexpr uint32_t kMaxContactConstraints = 1024;
constexpr size_t kTempAllocatorBytes = 10 * 1024 * 1024;

class BroadPhaseLayerMapper final : public JPH::BroadPhaseLayerInterface
{
public:
	BroadPhaseLayerMapper()
	{
		m_layers[ObjectLayers::Static] = JPH::BroadPhaseLayer(BroadPhaseLayers::NonMoving);
		m_layers[ObjectLayers::Dynamic] = JPH::BroadPhaseLayer(BroadPhaseLayers::Moving);
	}

	JPH::uint GetNumBroadPhaseLayers() const override
	{
		return BroadPhaseLayers::Count;
	}

	JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
	{
		return m_layers[layer];
	}

private:
	JPH::BroadPhaseLayer m_layers[ObjectLayers::Count]{};
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
	bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const override
	{
		if (layer == ObjectLayers::Static)
		{
			return broadPhaseLayer == JPH::BroadPhaseLayer(BroadPhaseLayers::Moving);
		}

		return layer == ObjectLayers::Dynamic;
	}
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
{
public:
	bool ShouldCollide(JPH::ObjectLayer layerA, JPH::ObjectLayer layerB) const override
	{
		if (layerA == ObjectLayers::Static)
		{
			return layerB == ObjectLayers::Dynamic;
		}

		return layerA == ObjectLayers::Dynamic;
	}
};

// Engine space is left-handed Y-up. Jolt is right-handed Y-up.
// Reflecting Z takes a position (x, y, z) to (x, y, -z) and a quaternion
// (x, y, z, w) to (x, y, -z, -w). Applying the map twice is the identity.
[[nodiscard]] JPH::Vec3 ToJoltPosition(const DirectX::XMFLOAT3& position)
{
	return JPH::Vec3(position.x, position.y, -position.z);
}

[[nodiscard]] DirectX::XMFLOAT3 FromJoltPosition(JPH::RVec3Arg position)
{
	return DirectX::XMFLOAT3(
		position.GetX(),
		position.GetY(),
		-position.GetZ());
}

[[nodiscard]] JPH::Quat ToJoltRotation(const DirectX::XMFLOAT4& rotation)
{
	return JPH::Quat(rotation.x, rotation.y, -rotation.z, -rotation.w);
}

[[nodiscard]] DirectX::XMFLOAT4 FromJoltRotation(JPH::QuatArg rotation)
{
	return DirectX::XMFLOAT4(
		rotation.GetX(),
		rotation.GetY(),
		-rotation.GetZ(),
		-rotation.GetW());
}

[[nodiscard]] bool IsPositive(const DirectX::XMFLOAT3& value)
{
	return value.x > 0.0f && value.y > 0.0f && value.z > 0.0f;
}

[[nodiscard]] PhysicsPose LerpPose(const PhysicsPose& from, const PhysicsPose& to, float alpha)
{
	const DirectX::XMVECTOR position = DirectX::XMVectorLerp(
		DirectX::XMLoadFloat3(&from.position),
		DirectX::XMLoadFloat3(&to.position),
		alpha);
	const DirectX::XMVECTOR rotation = DirectX::XMQuaternionSlerp(
		DirectX::XMLoadFloat4(&from.rotation),
		DirectX::XMLoadFloat4(&to.rotation),
		alpha);

	PhysicsPose pose{};
	DirectX::XMStoreFloat3(&pose.position, position);
	DirectX::XMStoreFloat4(&pose.rotation, rotation);
	return pose;
}
} // namespace

struct PhysicsWorld::Impl
{
	struct BodySlot
	{
		bool alive = false;
		uint32_t generation = 0;
		JPH::BodyID bodyId{};
		PhysicsPose previous{};
		PhysicsPose current{};
	};

	Impl()
	{
		system.Init(
			kMaxBodies,
			0,
			kMaxBodyPairs,
			kMaxContactConstraints,
			broadPhaseLayers,
			objectVsBroadPhase,
			objectPairs);
		system.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
	}

	[[nodiscard]] BodySlot* FindSlot(PhysicsBodyId id)
	{
		return const_cast<BodySlot*>(std::as_const(*this).FindSlot(id));
	}

	[[nodiscard]] const BodySlot* FindSlot(PhysicsBodyId id) const
	{
		if (!id.IsValid() || id.Index >= bodies.size())
		{
			return nullptr;
		}

		const BodySlot& slot = bodies[id.Index];
		if (!slot.alive || slot.generation != id.Generation)
		{
			return nullptr;
		}

		return &slot;
	}

	[[nodiscard]] PhysicsPose ReadPose(JPH::BodyID bodyId) const
	{
		JPH::RVec3 position = JPH::RVec3::sZero();
		JPH::Quat rotation = JPH::Quat::sIdentity();
		system.GetBodyInterface().GetPositionAndRotation(bodyId, position, rotation);

		PhysicsPose pose{};
		pose.position = FromJoltPosition(position);
		pose.rotation = FromJoltRotation(rotation);
		return pose;
	}

	[[nodiscard]] PhysicsBodyId AddSlot(JPH::BodyID bodyId, const PhysicsPose& pose)
	{
		uint32_t index = 0;
		if (!freeIndices.empty())
		{
			index = freeIndices.back();
			freeIndices.pop_back();
		}
		else
		{
			index = static_cast<uint32_t>(bodies.size());
			BodySlot created{};
			created.generation = 1;
			bodies.push_back(created);
		}

		BodySlot& slot = bodies[index];
		slot.alive = true;
		slot.bodyId = bodyId;
		slot.previous = pose;
		slot.current = pose;
		return PhysicsBodyId{ index, slot.generation };
	}

	BroadPhaseLayerMapper broadPhaseLayers{};
	ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhase{};
	ObjectLayerPairFilterImpl objectPairs{};
	JPH::TempAllocatorImpl tempAllocator{ kTempAllocatorBytes };
	JPH::JobSystemThreadPool jobSystem{
		static_cast<JPH::uint>(JPH::cMaxPhysicsJobs),
		static_cast<JPH::uint>(JPH::cMaxPhysicsBarriers) };
	JPH::PhysicsSystem system{};
	std::vector<BodySlot> bodies{};
	std::vector<uint32_t> freeIndices{};
};

PhysicsWorld::PhysicsWorld() = default;

PhysicsWorld::PhysicsWorld(std::unique_ptr<Impl> impl)
	: m_impl(std::move(impl))
{
}

PhysicsWorld::~PhysicsWorld() = default;

PhysicsWorld::PhysicsWorld(PhysicsWorld&& other) noexcept = default;

PhysicsWorld& PhysicsWorld::operator=(PhysicsWorld&& other) noexcept = default;

Result<PhysicsWorld> PhysicsWorld::Create()
{
	if (JPH::Factory::sInstance == nullptr)
	{
		return FailInternal<PhysicsWorld>(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			"PhysicsWorld requires InitializePhysicsLibrary");
	}

	return MakeOk(PhysicsWorld(std::make_unique<Impl>()));
}

Result<PhysicsBodyId> PhysicsWorld::CreateBox(const PhysicsBoxDesc& desc)
{
	if (m_impl == nullptr)
	{
		return FailInternal<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::CreateBox requires a created world");
	}

	if (!IsPositive(desc.halfExtent))
	{
		return FailInternal<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::CreateBox requires positive half extents");
	}

	const PhysicsPose pose{ desc.position, desc.rotation };
	JPH::BodyCreationSettings settings(
		new JPH::BoxShape(JPH::Vec3(desc.halfExtent.x, desc.halfExtent.y, desc.halfExtent.z)),
		ToJoltPosition(desc.position),
		ToJoltRotation(desc.rotation),
		JPH::EMotionType::Static,
		ObjectLayers::Static);
	settings.mRestitution = 0.0f;

	const JPH::BodyID bodyId = m_impl->system.GetBodyInterface().CreateAndAddBody(
		settings,
		JPH::EActivation::DontActivate);
	if (bodyId.IsInvalid())
	{
		return FailInternal<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			"PhysicsWorld::CreateBox failed to add a body");
	}

	return MakeOk(m_impl->AddSlot(bodyId, pose));
}

Result<PhysicsBodyId> PhysicsWorld::CreateSphere(const PhysicsSphereDesc& desc)
{
	if (m_impl == nullptr)
	{
		return FailInternal<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::CreateSphere requires a created world");
	}

	if (desc.radius <= 0.0f)
	{
		return FailInternal<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::CreateSphere requires a positive radius");
	}

	const PhysicsPose pose{ desc.position, desc.rotation };
	JPH::BodyCreationSettings settings(
		new JPH::SphereShape(desc.radius),
		ToJoltPosition(desc.position),
		ToJoltRotation(desc.rotation),
		JPH::EMotionType::Dynamic,
		ObjectLayers::Dynamic);
	settings.mRestitution = 0.0f;

	const JPH::BodyID bodyId = m_impl->system.GetBodyInterface().CreateAndAddBody(
		settings,
		JPH::EActivation::Activate);
	if (bodyId.IsInvalid())
	{
		return FailInternal<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			"PhysicsWorld::CreateSphere failed to add a body");
	}

	return MakeOk(m_impl->AddSlot(bodyId, pose));
}

Result<void> PhysicsWorld::Step(float deltaSeconds)
{
	if (m_impl == nullptr)
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::Step requires a created world");
	}

	if (deltaSeconds <= 0.0f)
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::Step requires a positive delta");
	}

	JPH::BodyInterface& bodies = m_impl->system.GetBodyInterface();
	for (Impl::BodySlot& slot : m_impl->bodies)
	{
		if (!slot.alive)
		{
			continue;
		}

		slot.previous = m_impl->ReadPose(slot.bodyId);
	}

	const JPH::EPhysicsUpdateError error = m_impl->system.Update(
		deltaSeconds,
		1,
		&m_impl->tempAllocator,
		&m_impl->jobSystem);
	if (error != JPH::EPhysicsUpdateError::None)
	{
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			"PhysicsWorld::Step failed");
	}

	for (Impl::BodySlot& slot : m_impl->bodies)
	{
		if (!slot.alive)
		{
			continue;
		}

		slot.current = m_impl->ReadPose(slot.bodyId);
	}

	return MakeOk();
}

bool PhysicsWorld::Sample(PhysicsBodyId id, float alpha, PhysicsPose& outPose) const
{
	if (m_impl == nullptr)
	{
		return false;
	}

	const Impl::BodySlot* slot = m_impl->FindSlot(id);
	if (slot == nullptr)
	{
		return false;
	}

	float clampedAlpha = alpha;
	if (clampedAlpha < 0.0f)
	{
		clampedAlpha = 0.0f;
	}
	else if (clampedAlpha > 1.0f)
	{
		clampedAlpha = 1.0f;
	}

	outPose = LerpPose(slot->previous, slot->current, clampedAlpha);
	return true;
}

bool PhysicsWorld::GetLinearVelocity(PhysicsBodyId id, DirectX::XMFLOAT3& outVelocity) const
{
	if (m_impl == nullptr)
	{
		return false;
	}

	const Impl::BodySlot* slot = m_impl->FindSlot(id);
	if (slot == nullptr)
	{
		return false;
	}

	const JPH::Vec3 velocity = m_impl->system.GetBodyInterface().GetLinearVelocity(slot->bodyId);
	outVelocity = DirectX::XMFLOAT3(velocity.GetX(), velocity.GetY(), -velocity.GetZ());
	return true;
}
