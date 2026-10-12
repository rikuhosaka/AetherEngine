#include "Engine/Physics/PhysicsWorld.h"

#include "Engine/Core/Log/Result.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
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
[[nodiscard]] JPH::Vec3 ToJoltVector(const DirectX::XMFLOAT3& value)
{
	return JPH::Vec3(value.x, value.y, -value.z);
}

[[nodiscard]] JPH::Vec3 ToJoltPosition(const DirectX::XMFLOAT3& position)
{
	return ToJoltVector(position);
}

[[nodiscard]] DirectX::XMFLOAT3 FromJoltPosition(JPH::RVec3Arg position)
{
	return DirectX::XMFLOAT3(
		position.GetX(),
		position.GetY(),
		-position.GetZ());
}

[[nodiscard]] DirectX::XMFLOAT3 FromJoltVector(JPH::Vec3Arg value)
{
	return DirectX::XMFLOAT3(value.GetX(), value.GetY(), -value.GetZ());
}

[[nodiscard]] uint64_t PackBodyUserData(PhysicsBodyId id)
{
	return (static_cast<uint64_t>(id.Generation) << 32) | static_cast<uint64_t>(id.Index);
}

[[nodiscard]] PhysicsBodyId UnpackBodyUserData(uint64_t userData)
{
	return PhysicsBodyId{
		static_cast<uint32_t>(userData),
		static_cast<uint32_t>(userData >> 32),
	};
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

[[nodiscard]] bool IsKnownMotion(PhysicsMotion motion)
{
	switch (motion)
	{
	case PhysicsMotion::Static:
	case PhysicsMotion::Dynamic:
	case PhysicsMotion::Kinematic:
		return true;
	}

	return false;
}

[[nodiscard]] bool IsKnownShape(PhysicsShapeType type)
{
	switch (type)
	{
	case PhysicsShapeType::Box:
	case PhysicsShapeType::Sphere:
	case PhysicsShapeType::Capsule:
	case PhysicsShapeType::ConvexHull:
	case PhysicsShapeType::Mesh:
		return true;
	}

	return false;
}

struct MotionBinding
{
	JPH::EMotionType motionType = JPH::EMotionType::Static;
	JPH::ObjectLayer layer = ObjectLayers::Static;
	JPH::EActivation activation = JPH::EActivation::DontActivate;
};

[[nodiscard]] MotionBinding BindMotion(PhysicsMotion motion)
{
	MotionBinding binding{};
	if (motion == PhysicsMotion::Dynamic)
	{
		binding.motionType = JPH::EMotionType::Dynamic;
		binding.layer = ObjectLayers::Dynamic;
		binding.activation = JPH::EActivation::Activate;
	}
	else if (motion == PhysicsMotion::Kinematic)
	{
		binding.motionType = JPH::EMotionType::Kinematic;
		binding.layer = ObjectLayers::Dynamic;
		binding.activation = JPH::EActivation::Activate;
	}

	return binding;
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

[[nodiscard]] Result<JPH::RefConst<JPH::Shape>> CreateCollisionShape(const PhysicsBodyDesc& desc)
{
	switch (desc.shape.type)
	{
	case PhysicsShapeType::Box:
		if (!IsPositive(desc.shape.halfExtent))
		{
			return MakeFail<JPH::RefConst<JPH::Shape>>(
				ErrorCode::InvalidArgument,
				"PhysicsWorld::CreateBody requires positive half extents");
		}

		return MakeOk<JPH::RefConst<JPH::Shape>>(new JPH::BoxShape(
			JPH::Vec3(desc.shape.halfExtent.x, desc.shape.halfExtent.y, desc.shape.halfExtent.z)));

	case PhysicsShapeType::Sphere:
		if (desc.shape.radius <= 0.0f)
		{
			return MakeFail<JPH::RefConst<JPH::Shape>>(
				ErrorCode::InvalidArgument,
				"PhysicsWorld::CreateBody requires a positive radius");
		}

		return MakeOk<JPH::RefConst<JPH::Shape>>(new JPH::SphereShape(desc.shape.radius));

	case PhysicsShapeType::Capsule:
		if (desc.shape.radius <= 0.0f || desc.shape.halfHeight <= 0.0f)
		{
			return MakeFail<JPH::RefConst<JPH::Shape>>(
				ErrorCode::InvalidArgument,
				"PhysicsWorld::CreateBody requires a positive capsule radius and half height");
		}

		return MakeOk<JPH::RefConst<JPH::Shape>>(
			new JPH::CapsuleShape(desc.shape.halfHeight, desc.shape.radius));

	case PhysicsShapeType::ConvexHull:
		if (desc.shape.points.size() < 4)
		{
			return MakeFail<JPH::RefConst<JPH::Shape>>(
				ErrorCode::InvalidArgument,
				"PhysicsWorld::CreateBody requires at least four convex hull points");
		}

		{
			JPH::Array<JPH::Vec3> hullPoints;
			hullPoints.reserve(static_cast<JPH::uint>(desc.shape.points.size()));
			for (const DirectX::XMFLOAT3& point : desc.shape.points)
			{
				hullPoints.push_back(ToJoltVector(point));
			}

			const JPH::ConvexHullShapeSettings settings(hullPoints);
			const JPH::ShapeSettings::ShapeResult created = settings.Create();
			if (!created.IsValid())
			{
				return MakeFail<JPH::RefConst<JPH::Shape>>(
					ErrorCode::InvalidArgument,
					"PhysicsWorld::CreateBody could not build a convex hull");
			}

			return MakeOk<JPH::RefConst<JPH::Shape>>(created.Get().GetPtr());
		}

	case PhysicsShapeType::Mesh:
		if (desc.motion != PhysicsMotion::Static)
		{
			return MakeFail<JPH::RefConst<JPH::Shape>>(
				ErrorCode::InvalidArgument,
				"PhysicsWorld::CreateBody requires a static mesh");
		}

		if (desc.shape.indices.empty() || desc.shape.indices.size() % 3 != 0)
		{
			return MakeFail<JPH::RefConst<JPH::Shape>>(
				ErrorCode::InvalidArgument,
				"PhysicsWorld::CreateBody requires triangle indices in multiples of three");
		}

		{
			JPH::VertexList vertices;
			vertices.reserve(static_cast<JPH::uint>(desc.shape.points.size()));
			for (const DirectX::XMFLOAT3& point : desc.shape.points)
			{
				const JPH::Vec3 converted = ToJoltVector(point);
				vertices.push_back(JPH::Float3(converted.GetX(), converted.GetY(), converted.GetZ()));
			}

			JPH::IndexedTriangleList triangles;
			triangles.reserve(static_cast<JPH::uint>(desc.shape.indices.size() / 3));
			for (size_t index = 0; index < desc.shape.indices.size(); index += 3)
			{
				const uint32_t first = desc.shape.indices[index];
				const uint32_t second = desc.shape.indices[index + 1];
				const uint32_t third = desc.shape.indices[index + 2];
				if (first >= desc.shape.points.size()
					|| second >= desc.shape.points.size()
					|| third >= desc.shape.points.size())
				{
					return MakeFail<JPH::RefConst<JPH::Shape>>(
						ErrorCode::InvalidArgument,
						"PhysicsWorld::CreateBody requires in-range triangle indices");
				}

				triangles.push_back(JPH::IndexedTriangle(first, third, second));
			}

			JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles));
			if (settings.mIndexedTriangles.empty())
			{
				return MakeFail<JPH::RefConst<JPH::Shape>>(
					ErrorCode::InvalidArgument,
					"PhysicsWorld::CreateBody requires at least one triangle");
			}

			const JPH::ShapeSettings::ShapeResult created = settings.Create();
			if (!created.IsValid())
			{
				return MakeFail<JPH::RefConst<JPH::Shape>>(
					ErrorCode::InvalidArgument,
					"PhysicsWorld::CreateBody requires at least one triangle");
			}

			return MakeOk<JPH::RefConst<JPH::Shape>>(created.Get().GetPtr());
		}
	}

	return MakeFail<JPH::RefConst<JPH::Shape>>(
		ErrorCode::InvalidArgument,
		"PhysicsWorld::CreateBody requires a known shape");
}
} // namespace

struct PhysicsWorld::Impl
{
	struct BodySlot
	{
		bool alive = false;
		uint32_t generation = 0;
		PhysicsMotion motion = PhysicsMotion::Static;
		JPH::BodyID bodyId{};
		PhysicsPose previous{};
		PhysicsPose current{};
	};

	struct OpenContact
	{
		JPH::BodyID joltA{};
		JPH::BodyID joltB{};
		PhysicsBodyId idA{};
		PhysicsBodyId idB{};
		PhysicsMotion motionA = PhysicsMotion::Static;
		PhysicsMotion motionB = PhysicsMotion::Static;
		PhysicsContact contact{};
	};

	class ContactRecorder final : public JPH::ContactListener
	{
	public:
		explicit ContactRecorder(Impl* owner)
			: m_owner(owner)
		{
		}

		void OnContactAdded(
			const JPH::Body& inBody1,
			const JPH::Body& inBody2,
			const JPH::ContactManifold& inManifold,
			JPH::ContactSettings& ioSettings) override;

	private:
		Impl* m_owner = nullptr;
	};

	Impl()
		: recorder(this)
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
		system.SetContactListener(&recorder);
	}

	~Impl()
	{
		system.SetContactListener(nullptr);
	}

	Impl(const Impl&) = delete;
	Impl& operator=(const Impl&) = delete;

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

	[[nodiscard]] PhysicsBodyId AddSlot(JPH::BodyID bodyId, const PhysicsPose& pose, PhysicsMotion motion)
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
		slot.motion = motion;
		slot.bodyId = bodyId;
		slot.previous = pose;
		slot.current = pose;
		return PhysicsBodyId{ index, slot.generation };
	}

	void ReleaseSlot(PhysicsBodyId id)
	{
		BodySlot* slot = FindSlot(id);
		if (slot == nullptr)
		{
			return;
		}

		slot->alive = false;
		slot->motion = PhysicsMotion::Static;
		slot->bodyId = JPH::BodyID{};
		slot->previous = {};
		slot->current = {};
		++slot->generation;
		freeIndices.push_back(id.Index);
	}

	void NoteContactAdded(
		const JPH::Body& body1,
		const JPH::Body& body2,
		const JPH::ContactManifold& manifold)
	{
		const PhysicsBodyId idA = UnpackBodyUserData(body1.GetUserData());
		const PhysicsBodyId idB = UnpackBodyUserData(body2.GetUserData());
		const BodySlot* slotA = FindSlot(idA);
		const BodySlot* slotB = FindSlot(idB);
		if (slotA == nullptr || slotB == nullptr)
		{
			return;
		}

		std::lock_guard<std::mutex> lock(contactMutex);
		for (const OpenContact& open : openContacts)
		{
			if (open.joltA == body1.GetID() && open.joltB == body2.GetID())
			{
				return;
			}
		}

		const JPH::RVec3 point = manifold.mRelativeContactPointsOn1.size() > 0
			? manifold.GetWorldSpaceContactPointOn1(0)
			: manifold.mBaseOffset;

		OpenContact created{};
		created.joltA = body1.GetID();
		created.joltB = body2.GetID();
		created.idA = idA;
		created.idB = idB;
		created.motionA = slotA->motion;
		created.motionB = slotB->motion;
		created.contact.first = idA;
		created.contact.second = idB;
		created.contact.point = FromJoltPosition(point);
		created.contact.normal = FromJoltVector(manifold.mWorldSpaceNormal);
		began.push_back(created.contact);
		openContacts.push_back(created);
	}

	[[nodiscard]] bool DynamicIsActive(PhysicsBodyId id, PhysicsMotion motion) const
	{
		if (motion != PhysicsMotion::Dynamic)
		{
			return false;
		}

		const BodySlot* slot = FindSlot(id);
		if (slot == nullptr)
		{
			return false;
		}

		return system.GetBodyInterface().IsActive(slot->bodyId);
	}

	void ReconcileContacts()
	{
		std::lock_guard<std::mutex> lock(contactMutex);
		std::vector<OpenContact> stillOpen;
		stillOpen.reserve(openContacts.size());
		for (const OpenContact& open : openContacts)
		{
			const bool firstAlive = FindSlot(open.idA) != nullptr;
			const bool secondAlive = FindSlot(open.idB) != nullptr;
			if (!firstAlive || !secondAlive)
			{
				ended.push_back(open.contact);
				continue;
			}

			if (system.WereBodiesInContact(open.joltA, open.joltB))
			{
				stillOpen.push_back(open);
				continue;
			}

			if (DynamicIsActive(open.idA, open.motionA) || DynamicIsActive(open.idB, open.motionB))
			{
				ended.push_back(open.contact);
				continue;
			}

			stillOpen.push_back(open);
		}

		openContacts.swap(stillOpen);
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
	std::mutex contactMutex{};
	std::vector<OpenContact> openContacts{};
	std::vector<PhysicsContact> began{};
	std::vector<PhysicsContact> ended{};
	ContactRecorder recorder;
};

void PhysicsWorld::Impl::ContactRecorder::OnContactAdded(
	const JPH::Body& inBody1,
	const JPH::Body& inBody2,
	const JPH::ContactManifold& inManifold,
	JPH::ContactSettings& /*ioSettings*/)
{
	m_owner->NoteContactAdded(inBody1, inBody2, inManifold);
}

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

Result<PhysicsBodyId> PhysicsWorld::CreateBody(const PhysicsBodyDesc& desc)
{
	if (m_impl == nullptr)
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::CreateBody requires a created world");
	}

	if (!IsKnownMotion(desc.motion))
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::CreateBody requires a known motion");
	}

	if (!IsKnownShape(desc.shape.type))
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::CreateBody requires a known shape");
	}

	if (desc.friction < 0.0f)
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::CreateBody requires non-negative friction");
	}

	if (desc.restitution < 0.0f)
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::CreateBody requires non-negative restitution");
	}

	if (desc.mass < 0.0f)
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::CreateBody requires non-negative mass");
	}

	Result<JPH::RefConst<JPH::Shape>> shape = CreateCollisionShape(desc);
	if (!shape)
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			shape.error.code,
			shape.error.message);
	}

	const MotionBinding binding = BindMotion(desc.motion);
	JPH::BodyCreationSettings settings(
		shape.value.GetPtr(),
		ToJoltPosition(desc.pose.position),
		ToJoltRotation(desc.pose.rotation),
		binding.motionType,
		binding.layer);
	settings.mFriction = desc.friction;
	settings.mRestitution = desc.restitution;
	if (desc.mass > 0.0f)
	{
		settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		settings.mMassPropertiesOverride.mMass = desc.mass;
	}

	const JPH::BodyID bodyId = m_impl->system.GetBodyInterface().CreateAndAddBody(
		settings,
		binding.activation);
	if (bodyId.IsInvalid())
	{
		return FailInternal<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			"PhysicsWorld::CreateBody failed to add a body");
	}

	const PhysicsBodyId id = m_impl->AddSlot(bodyId, desc.pose, desc.motion);
	m_impl->system.GetBodyInterface().SetUserData(bodyId, PackBodyUserData(id));
	return MakeOk(id);
}

Result<void> PhysicsWorld::Destroy(PhysicsBodyId id)
{
	if (m_impl == nullptr)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::Destroy requires a created world");
	}

	Impl::BodySlot* slot = m_impl->FindSlot(id);
	if (slot == nullptr)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::Destroy requires a live body");
	}

	JPH::BodyInterface& bodies = m_impl->system.GetBodyInterface();
	bodies.RemoveBody(slot->bodyId);
	bodies.DestroyBody(slot->bodyId);
	m_impl->ReleaseSlot(id);
	return MakeOk();
}

Result<void> PhysicsWorld::SetPose(PhysicsBodyId id, const PhysicsPose& pose)
{
	if (m_impl == nullptr)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::SetPose requires a created world");
	}

	Impl::BodySlot* slot = m_impl->FindSlot(id);
	if (slot == nullptr)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::SetPose requires a live body");
	}

	const JPH::EActivation activation = slot->motion == PhysicsMotion::Static
		? JPH::EActivation::DontActivate
		: JPH::EActivation::Activate;
	m_impl->system.GetBodyInterface().SetPositionAndRotation(
		slot->bodyId,
		ToJoltPosition(pose.position),
		ToJoltRotation(pose.rotation),
		activation);
	slot->previous = pose;
	slot->current = pose;
	return MakeOk();
}

Result<void> PhysicsWorld::MoveKinematic(PhysicsBodyId id, const PhysicsPose& target, float deltaSeconds)
{
	if (m_impl == nullptr)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::MoveKinematic requires a created world");
	}

	if (deltaSeconds <= 0.0f)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::MoveKinematic requires a positive delta");
	}

	Impl::BodySlot* slot = m_impl->FindSlot(id);
	if (slot == nullptr)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::MoveKinematic requires a live body");
	}

	if (slot->motion != PhysicsMotion::Kinematic)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::MoveKinematic requires a kinematic body");
	}

	m_impl->system.GetBodyInterface().MoveKinematic(
		slot->bodyId,
		ToJoltPosition(target.position),
		ToJoltRotation(target.rotation),
		deltaSeconds);
	return MakeOk();
}

Result<void> PhysicsWorld::SetLinearVelocity(PhysicsBodyId id, const DirectX::XMFLOAT3& velocity)
{
	if (m_impl == nullptr)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::SetLinearVelocity requires a created world");
	}

	Impl::BodySlot* slot = m_impl->FindSlot(id);
	if (slot == nullptr)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::SetLinearVelocity requires a live body");
	}

	if (slot->motion != PhysicsMotion::Dynamic)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::SetLinearVelocity requires a dynamic body");
	}

	m_impl->system.GetBodyInterface().SetLinearVelocity(slot->bodyId, ToJoltVector(velocity));
	return MakeOk();
}

Result<void> PhysicsWorld::AddImpulse(PhysicsBodyId id, const DirectX::XMFLOAT3& impulse)
{
	if (m_impl == nullptr)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::AddImpulse requires a created world");
	}

	Impl::BodySlot* slot = m_impl->FindSlot(id);
	if (slot == nullptr)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::AddImpulse requires a live body");
	}

	if (slot->motion != PhysicsMotion::Dynamic)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::AddImpulse requires a dynamic body");
	}

	m_impl->system.GetBodyInterface().AddImpulse(slot->bodyId, ToJoltVector(impulse));
	return MakeOk();
}

Result<void> PhysicsWorld::SetGravity(const DirectX::XMFLOAT3& gravity)
{
	if (m_impl == nullptr)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::SetGravity requires a created world");
	}

	m_impl->system.SetGravity(ToJoltVector(gravity));
	return MakeOk();
}

Result<void> PhysicsWorld::Step(float deltaSeconds)
{
	if (m_impl == nullptr)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::Step requires a created world");
	}

	if (deltaSeconds <= 0.0f)
	{
		return FailRuntime(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsWorld::Step requires a positive delta");
	}

	{
		std::lock_guard<std::mutex> lock(m_impl->contactMutex);
		m_impl->began.clear();
		m_impl->ended.clear();
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

	m_impl->ReconcileContacts();

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

std::span<const PhysicsContact> PhysicsWorld::GetContactsBegan() const
{
	if (m_impl == nullptr)
	{
		return {};
	}

	return m_impl->began;
}

std::span<const PhysicsContact> PhysicsWorld::GetContactsEnded() const
{
	if (m_impl == nullptr)
	{
		return {};
	}

	return m_impl->ended;
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
	outVelocity = FromJoltVector(velocity);
	return true;
}

bool PhysicsWorld::RayCast(
	const DirectX::XMFLOAT3& origin,
	const DirectX::XMFLOAT3& direction,
	float maxDistance,
	PhysicsBodyId ignore,
	PhysicsRayHit& outHit) const
{
	if (m_impl == nullptr || !(maxDistance > 0.0f))
	{
		return false;
	}

	const DirectX::XMVECTOR loadedDirection = DirectX::XMLoadFloat3(&direction);
	const float length = DirectX::XMVectorGetX(DirectX::XMVector3Length(loadedDirection));
	if (!(length > 0.0f))
	{
		return false;
	}

	DirectX::XMFLOAT3 scaledDirection{};
	DirectX::XMStoreFloat3(
		&scaledDirection,
		DirectX::XMVectorScale(loadedDirection, maxDistance / length));

	const JPH::RRayCast ray(ToJoltPosition(origin), ToJoltVector(scaledDirection));
	JPH::RayCastResult hit;
	const Impl::BodySlot* ignored = m_impl->FindSlot(ignore);
	const bool found = ignored == nullptr
		? m_impl->system.GetNarrowPhaseQuery().CastRay(ray, hit)
		: m_impl->system.GetNarrowPhaseQuery().CastRay(
			ray,
			hit,
			{},
			{},
			JPH::IgnoreSingleBodyFilter(ignored->bodyId));
	if (!found)
	{
		return false;
	}

	const JPH::RVec3 point = ray.GetPointOnRay(hit.mFraction);
	JPH::BodyLockRead lock(m_impl->system.GetBodyLockInterface(), hit.mBodyID);
	if (!lock.Succeeded())
	{
		return false;
	}

	const JPH::Body& body = lock.GetBody();
	const PhysicsBodyId id = UnpackBodyUserData(body.GetUserData());
	if (m_impl->FindSlot(id) == nullptr)
	{
		return false;
	}

	const JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point);
	outHit.body = id;
	outHit.fraction = hit.mFraction;
	outHit.position = FromJoltPosition(point);
	outHit.normal = FromJoltVector(normal);
	return true;
}
