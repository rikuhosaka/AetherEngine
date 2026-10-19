#include "Engine/World/Test/EntityPhysicsTest.h"

#include "Engine/Core/Log/Result.h"
#include "Engine/World/EntityPhysics.h"
#include "Engine/Physics/PhysicsLibrary.h"
#include "Engine/World/World.h"

#include <cmath>
#include <format>

namespace
{
[[nodiscard]] bool IsNearlyEqual(float left, float right, float epsilon)
{
	return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] Result<EntityPhysics> CreateSimulation()
{
	return EntityPhysics::Create();
}

[[nodiscard]] Result<void> TestBoundSphereFalls()
{
	Result<EntityPhysics> created = CreateSimulation();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	EntityPhysics& simulation = created.value;
	World world{};
	WorldSpawnDesc spawn{};
	spawn.transform.position = { 0.0f, 4.0f, 0.0f };
	spawn.transform.scale = { 2.0f, 3.0f, 4.0f };
	const EntityId entity = world.Spawn(spawn);

	PhysicsBodyDesc sphere{};
	sphere.motion = PhysicsMotion::Dynamic;
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	Result<PhysicsBodyId> body = simulation.Bind(world, entity, sphere);
	if (!body)
	{
		return FailInternal(LogCategory::Physics, body.error.code, body.error.message);
	}

	constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
	constexpr int kStepCount = 30;
	for (int stepIndex = 0; stepIndex < kStepCount; ++stepIndex)
	{
		if (auto step = simulation.Step(world, kFixedDeltaSeconds); !step)
		{
			return step;
		}
	}

	simulation.WriteTransforms(world, 1.0f);
	const Transform* transform = world.GetTransform(entity);
	if (transform == nullptr
		|| !(transform->position.y < 3.5f)
		|| !IsNearlyEqual(transform->scale.x, 2.0f, 0.0001f)
		|| !IsNearlyEqual(transform->scale.y, 3.0f, 0.0001f)
		|| !IsNearlyEqual(transform->scale.z, 4.0f, 0.0001f))
	{
		const float positionY = transform == nullptr ? 0.0f : transform->position.y;
		return FailInternal(
			LogCategory::Physics,
			ErrorCode::RuntimeError,
			std::format("Bound sphere should fall without changing scale, position.y={}", positionY));
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestBindRejectsParentAndDuplicate()
{
	Result<EntityPhysics> created = CreateSimulation();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	EntityPhysics& simulation = created.value;
	World world{};
	const EntityId parent = world.Spawn({});
	WorldSpawnDesc childDesc{};
	childDesc.parent = parent;
	const EntityId child = world.Spawn(childDesc);

	PhysicsBodyDesc sphere{};
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	if (simulation.Bind(world, child, sphere))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Bind should reject a parented entity");
	}

	const EntityId root = world.Spawn({});
	if (auto bound = simulation.Bind(world, root, sphere); !bound)
	{
		return FailInternal(LogCategory::Physics, bound.error.code, bound.error.message);
	}

	if (simulation.Bind(world, root, sphere))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Bind should reject an entity that is already bound");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestUnbindKeepsEntity()
{
	Result<EntityPhysics> created = CreateSimulation();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	EntityPhysics& simulation = created.value;
	World world{};
	const EntityId entity = world.Spawn({});
	PhysicsBodyDesc sphere{};
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	Result<PhysicsBodyId> body = simulation.Bind(world, entity, sphere);
	if (!body)
	{
		return FailInternal(LogCategory::Physics, body.error.code, body.error.message);
	}

	if (auto unbound = simulation.Unbind(entity); !unbound)
	{
		return unbound;
	}

	PhysicsPose pose{};
	if (!world.IsAlive(entity) || simulation.GetWorld().Sample(body.value, 1.0f, pose))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Unbind should keep the entity and destroy the body");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestDestroyEntityRemovesBody()
{
	Result<EntityPhysics> created = CreateSimulation();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	EntityPhysics& simulation = created.value;
	World world{};
	const EntityId entity = world.Spawn({});
	PhysicsBodyDesc sphere{};
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	Result<PhysicsBodyId> body = simulation.Bind(world, entity, sphere);
	if (!body)
	{
		return FailInternal(LogCategory::Physics, body.error.code, body.error.message);
	}

	world.Destroy(entity);
	if (auto step = simulation.Step(world, 1.0f / 60.0f); !step)
	{
		return step;
	}

	PhysicsPose pose{};
	if (world.IsAlive(entity) || simulation.GetWorld().Sample(body.value, 1.0f, pose))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Destroying the entity should remove its body on the next step");
	}

	return MakeOk();
}
} // namespace

Result<void> RunEntityPhysicsTests()
{
	if (auto library = InitializePhysicsLibrary(); !library)
	{
		return library;
	}

	if (auto fall = TestBoundSphereFalls(); !fall)
	{
		return fall;
	}

	if (auto rejected = TestBindRejectsParentAndDuplicate(); !rejected)
	{
		return rejected;
	}

	if (auto unbound = TestUnbindKeepsEntity(); !unbound)
	{
		return unbound;
	}

	return TestDestroyEntityRemovesBody();
}
