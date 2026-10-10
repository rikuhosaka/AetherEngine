#include "Engine/World/Test/WorldTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/World/World.h"

#include <cstddef>
#include <utility>

namespace
{
[[nodiscard]] size_t CountAlive(const World& world)
{
	size_t count = 0;
	world.ForEachAlive([&count](EntityId, const Transform&, const Renderable&) {
		++count;
	});
	return count;
}
} // namespace

Result<void> RunWorldTests()
{
	World world{};
	WorldSpawnDesc spawnDesc{};
	spawnDesc.transform.position = { 5.0f, 2.0f, -1.0f };
	spawnDesc.renderable.visible = true;

	const EntityId first = world.Spawn(std::move(spawnDesc));
	if (!world.IsAlive(first) || world.GetTransform(first) == nullptr || world.GetRenderable(first) == nullptr)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Spawn should produce a live entity");
	}

	const Transform* transform = world.GetTransform(first);
	if (transform->position.x != 5.0f || transform->position.y != 2.0f || transform->position.z != -1.0f)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Spawn should store the entity transform");
	}

	if (!world.GetRenderable(first)->visible || CountAlive(world) != 1)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Spawn should store a visible renderable");
	}

	world.Destroy(first);
	if (world.IsAlive(first) || world.GetTransform(first) != nullptr || CountAlive(world) != 0)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Destroyed entity should be invalid");
	}

	WorldSpawnDesc reusedDesc{};
	const EntityId second = world.Spawn(std::move(reusedDesc));
	if (!second.IsValid() || second.Index != first.Index)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Spawn should reuse the freed slot");
	}

	if (second.Generation == first.Generation)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Reused slot should have a new generation");
	}

	if (world.IsAlive(first))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Old entity id should stay invalid after reuse");
	}

	bool sawSecond = false;
	world.ForEachAlive([&](EntityId id, const Transform&, const Renderable&) {
		sawSecond = id.Index == second.Index && id.Generation == second.Generation;
	});
	if (!sawSecond || CountAlive(world) != 1)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Iteration should use the reused entity generation");
	}

	LOG_INFO(LogCategory::ECS, "WorldTests passed");
	return MakeOk();
}
