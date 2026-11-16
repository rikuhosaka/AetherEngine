#include "Runtime/Engine/World/PhysicsScene.h"

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/World/Component/RigidBody.h"

namespace
{
[[nodiscard]] PhysicsBodyDesc MakeBodyDesc(const RigidBody& body)
{
	PhysicsBodyDesc desc{};
	desc.motion = body.motion;
	desc.friction = body.friction;
	desc.restitution = body.restitution;
	desc.mass = body.mass;
	desc.shape.type = body.shape.type;
	desc.shape.halfExtent = body.shape.halfExtent;
	desc.shape.radius = body.shape.radius;
	desc.shape.halfHeight = body.shape.halfHeight;
	desc.shape.points = body.shape.points;
	desc.shape.indices = body.shape.indices;
	return desc;
}
} // namespace

PhysicsScene::PhysicsScene(PhysicsWorld world)
	: m_physicsWorld(std::move(world))
{
}

Result<PhysicsScene> PhysicsScene::Create()
{
	Result<PhysicsWorld> world = PhysicsWorld::Create();
	if (!world)
	{
		return MakeFail<PhysicsScene>(world.error.code, world.error.message);
	}

	return MakeOk(PhysicsScene(std::move(world.value)));
}

Result<PhysicsBodyId> PhysicsScene::Bind(World& world, EntityId entity, PhysicsBodyDesc desc)
{
	if (!world.IsAlive(entity))
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsScene::Bind requires a live entity");
	}

	if (world.GetParent(entity).IsValid())
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsScene::Bind requires a parentless entity");
	}

	if (IsBound(entity))
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsScene::Bind requires an unbound entity");
	}

	const Transform* transform = world.GetTransform(entity);
	if (transform == nullptr)
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"PhysicsScene::Bind requires a live entity");
	}

	desc.pose.position = transform->position;
	desc.pose.rotation = transform->rotation;
	Result<PhysicsBodyId> body = m_physicsWorld.CreateBody(desc);
	if (!body)
	{
		return MakeFail<PhysicsBodyId>(body.error.code, body.error.message);
	}

	m_bindings.push_back(Binding{ entity, body.value, desc.motion });
	return body;
}

Result<void> PhysicsScene::Unbind(EntityId entity)
{
	for (size_t index = 0; index < m_bindings.size(); ++index)
	{
		if (m_bindings[index].entity != entity)
		{
			continue;
		}

		const PhysicsBodyId body = m_bindings[index].body;
		if (auto destroyed = m_physicsWorld.Destroy(body); !destroyed)
		{
			return destroyed;
		}

		EraseAt(index);
		return MakeOk();
	}

	return FailRuntime(
		LogCategory::Physics,
		ErrorCode::InvalidArgument,
		"PhysicsScene::Unbind requires a bound entity");
}

Result<void> PhysicsScene::Step(World& world, float fixedDeltaSeconds)
{
	ReleaseIfEntityDead(world);
	for (const Binding& binding : m_bindings)
	{
		if (binding.motion != PhysicsMotion::Kinematic)
		{
			continue;
		}

		const Transform* transform = world.GetTransform(binding.entity);
		if (transform == nullptr)
		{
			continue;
		}

		PhysicsPose target{};
		target.position = transform->position;
		target.rotation = transform->rotation;
		if (auto moved = m_physicsWorld.MoveKinematic(binding.body, target, fixedDeltaSeconds); !moved)
		{
			return moved;
		}
	}

	return m_physicsWorld.Step(fixedDeltaSeconds);
}

void PhysicsScene::CommitTransforms(World& world)
{
	for (size_t index = 0; index < m_bindings.size();)
	{
		const Binding binding = m_bindings[index];
		Transform* transform = world.GetTransform(binding.entity);
		if (transform == nullptr)
		{
			(void)m_physicsWorld.Destroy(binding.body);
			EraseAt(index);
			continue;
		}

		PhysicsPose pose{};
		if (!m_physicsWorld.Sample(binding.body, 1.0f, pose))
		{
			EraseAt(index);
			continue;
		}

		transform->position = pose.position;
		transform->rotation = pose.rotation;
		++index;
	}
}

void PhysicsScene::SamplePoses(float alpha, std::vector<RenderPose>& out) const
{
	out.clear();
	out.reserve(m_bindings.size());
	for (const Binding& binding : m_bindings)
	{
		PhysicsPose pose{};
		if (!m_physicsWorld.Sample(binding.body, alpha, pose))
		{
			continue;
		}

		RenderPose render{};
		render.entity = binding.entity;
		render.position = pose.position;
		render.rotation = pose.rotation;
		out.push_back(render);
	}
}

PhysicsWorld& PhysicsScene::GetWorld()
{
	return m_physicsWorld;
}

bool PhysicsScene::IsBound(EntityId entity) const
{
	for (const Binding& binding : m_bindings)
	{
		if (binding.entity == entity)
		{
			return true;
		}
	}

	return false;
}

void PhysicsScene::ReleaseIfEntityDead(World& world)
{
	for (size_t index = 0; index < m_bindings.size();)
	{
		if (world.IsAlive(m_bindings[index].entity))
		{
			++index;
			continue;
		}

		(void)m_physicsWorld.Destroy(m_bindings[index].body);
		EraseAt(index);
	}
}

void PhysicsScene::EraseAt(size_t index)
{
	m_bindings[index] = m_bindings.back();
	m_bindings.pop_back();
}

Result<PhysicsScene> BindRigidBodies(World& world)
{
	Result<PhysicsScene> physics = PhysicsScene::Create();
	if (!physics)
	{
		return FailInternal<PhysicsScene>(LogCategory::Physics, physics.error.code, physics.error.message);
	}

	std::vector<EntityId> bodies{};
	world.ForEach<RigidBody>([&bodies](EntityId id, const RigidBody&) {
		bodies.push_back(id);
	});

	std::vector<EntityId> bound{};
	for (const EntityId id : bodies)
	{
		const RigidBody* body = world.Get<RigidBody>(id);
		if (body == nullptr)
		{
			continue;
		}

		const Result<PhysicsBodyId> created = physics.value.Bind(world, id, MakeBodyDesc(*body));
		if (!created)
		{
			for (const EntityId boundId : bound)
			{
				(void)physics.value.Unbind(boundId);
			}

			return FailInternal<PhysicsScene>(LogCategory::Physics, created.error.code, created.error.message);
		}

		bound.push_back(id);
	}

	return physics;
}
