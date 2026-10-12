#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/World/World.h"

#include <vector>

// Binds root entities to rigid bodies. The binding table lives outside World.
class EntityPhysics
{
public:
	EntityPhysics() = default;
	~EntityPhysics() = default;

	EntityPhysics(EntityPhysics&& other) noexcept = default;
	EntityPhysics& operator=(EntityPhysics&& other) noexcept = default;

	EntityPhysics(const EntityPhysics&) = delete;
	EntityPhysics& operator=(const EntityPhysics&) = delete;

	[[nodiscard]] static Result<EntityPhysics> Create();

	// Copies the entity position and rotation into the body. Scale is ignored.
	// The entity must be alive, parentless, and not already bound.
	[[nodiscard]] Result<PhysicsBodyId> Bind(World& world, EntityId entity, PhysicsBodyDesc desc);

	[[nodiscard]] Result<void> Unbind(EntityId entity);

	// Drops bodies whose entities are gone, then steps. Does not write transforms.
	[[nodiscard]] Result<void> Step(World& world, float fixedDeltaSeconds);

	// Writes interpolated position and rotation. Scale is left unchanged.
	void WriteTransforms(World& world, float alpha);

	[[nodiscard]] PhysicsWorld& GetWorld();

private:
	struct Binding
	{
		EntityId entity{};
		PhysicsBodyId body{};
	};

	explicit EntityPhysics(PhysicsWorld world);

	[[nodiscard]] bool IsBound(EntityId entity) const;
	void ReleaseIfEntityDead(World& world);
	void EraseAt(size_t index);

	PhysicsWorld m_world{};
	std::vector<Binding> m_bindings{};
};
