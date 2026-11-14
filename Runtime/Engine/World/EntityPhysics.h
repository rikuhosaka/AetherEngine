#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Physics/PhysicsWorld.h"
#include "Runtime/Engine/World/World.h"

#include <vector>

// 責任: エンティティと剛体の対応を保ち、ステップと姿勢の書き戻しを行う。
// 役割: 親の無いアクターを PhysicsWorld のボディへ束縛する。ステップは変換を書かず、書き戻しは別呼び出し。
// 所有: GameSubsystem が再生中だけ持つ。PhysicsWorld と束縛表を所有する。エンティティは World の所有のまま。コピー不可。
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

// Binds RigidBody components. A failure unbinds bodies from this call.
[[nodiscard]] Result<EntityPhysics> BindRigidBodies(World& world);
