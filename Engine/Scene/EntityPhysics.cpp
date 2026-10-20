#include "Engine/Scene/EntityPhysics.h"

#include "Engine/Core/Log/Result.h"

EntityPhysics::EntityPhysics(PhysicsWorld world)
	: m_world(std::move(world))
{
}

Result<EntityPhysics> EntityPhysics::Create()
{
	Result<PhysicsWorld> world = PhysicsWorld::Create();
	if (!world)
	{
		return MakeFail<EntityPhysics>(world.error.code, world.error.message);
	}

	return MakeOk(EntityPhysics(std::move(world.value)));
}

Result<PhysicsBodyId> EntityPhysics::Bind(World& world, EntityId entity, PhysicsBodyDesc desc)
{
	if (!world.IsAlive(entity))
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"EntityPhysics::Bind requires a live entity");
	}

	if (world.GetParent(entity).IsValid())
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"EntityPhysics::Bind requires a parentless entity");
	}

	if (IsBound(entity))
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"EntityPhysics::Bind requires an unbound entity");
	}

	const Transform* transform = world.GetTransform(entity);
	if (transform == nullptr)
	{
		return FailRuntime<PhysicsBodyId>(
			LogCategory::Physics,
			ErrorCode::InvalidArgument,
			"EntityPhysics::Bind requires a live entity");
	}

	desc.pose.position = transform->position;
	desc.pose.rotation = transform->rotation;
	Result<PhysicsBodyId> body = m_world.CreateBody(desc);
	if (!body)
	{
		return MakeFail<PhysicsBodyId>(body.error.code, body.error.message);
	}

	m_bindings.push_back(Binding{ entity, body.value });
	return body;
}

Result<void> EntityPhysics::Unbind(EntityId entity)
{
	for (size_t index = 0; index < m_bindings.size(); ++index)
	{
		if (m_bindings[index].entity != entity)
		{
			continue;
		}

		const PhysicsBodyId body = m_bindings[index].body;
		if (auto destroyed = m_world.Destroy(body); !destroyed)
		{
			return destroyed;
		}

		EraseAt(index);
		return MakeOk();
	}

	return FailRuntime(
		LogCategory::Physics,
		ErrorCode::InvalidArgument,
		"EntityPhysics::Unbind requires a bound entity");
}

Result<void> EntityPhysics::Step(World& world, float fixedDeltaSeconds)
{
	ReleaseIfEntityDead(world);
	return m_world.Step(fixedDeltaSeconds);
}

void EntityPhysics::WriteTransforms(World& world, float alpha)
{
	for (size_t index = 0; index < m_bindings.size();)
	{
		const Binding binding = m_bindings[index];
		Transform* transform = world.GetTransform(binding.entity);
		if (transform == nullptr)
		{
			(void)m_world.Destroy(binding.body);
			EraseAt(index);
			continue;
		}

		PhysicsPose pose{};
		if (!m_world.Sample(binding.body, alpha, pose))
		{
			EraseAt(index);
			continue;
		}

		transform->position = pose.position;
		transform->rotation = pose.rotation;
		++index;
	}
}

PhysicsWorld& EntityPhysics::GetWorld()
{
	return m_world;
}

bool EntityPhysics::IsBound(EntityId entity) const
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

void EntityPhysics::ReleaseIfEntityDead(World& world)
{
	for (size_t index = 0; index < m_bindings.size();)
	{
		if (world.IsAlive(m_bindings[index].entity))
		{
			++index;
			continue;
		}

		(void)m_world.Destroy(m_bindings[index].body);
		EraseAt(index);
	}
}

void EntityPhysics::EraseAt(size_t index)
{
	m_bindings[index] = m_bindings.back();
	m_bindings.pop_back();
}
