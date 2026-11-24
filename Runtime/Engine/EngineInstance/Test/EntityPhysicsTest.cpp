#include "Runtime/Engine/EngineInstance/Test/EntityPhysicsTest.h"

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Core/Math/Matrix.h"
#include "Runtime/Engine/EngineInstance/BuildRenderFrame.h"
#include "Runtime/Engine/World/Component/Renderable.h"
#include "Runtime/Engine/World/DebugStatsServices.h"
#include "Runtime/Engine/World/PhysicsScene.h"
#include "Runtime/Engine/World/World.h"
#include "Runtime/Engine/World/WorldCameraSample.h"
#include "Runtime/Physics/PhysicsLibrary.h"

#include <cmath>
#include <format>
#include <span>
#include <vector>

namespace
{
[[nodiscard]] bool IsNearlyEqual(float left, float right, float epsilon)
{
	return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] Result<PhysicsScene> CreateSimulation()
{
	return PhysicsScene::Create();
}

[[nodiscard]] Result<void> TestBoundSphereFalls()
{
	Result<PhysicsScene> created = CreateSimulation();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsScene& simulation = created.value;
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

	simulation.CommitTransforms(world);
	const TransformComponent* transform = world.GetTransform(entity);
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

[[nodiscard]] float ExtractedTranslationY(const ExtractedObjectSnapshot& object)
{
	const DirectX::XMMATRIX matrix = Aether::Math::LoadMatrixFromHlsl(object.worldMatrix);
	DirectX::XMFLOAT4X4 stored{};
	DirectX::XMStoreFloat4x4(&stored, matrix);
	return stored._42;
}

[[nodiscard]] const ExtractedObjectSnapshot* FindExtracted(
	const std::vector<ExtractedObjectSnapshot>& objects,
	EntityId id)
{
	for (const ExtractedObjectSnapshot& object : objects)
	{
		if (object.objectId.Index == id.Index && object.objectId.Generation == id.Generation)
		{
			return &object;
		}
	}

	return nullptr;
}

[[nodiscard]] std::vector<ExtractedObjectSnapshot> ExtractObjects(
	const World& world,
	std::span<const RenderPose> poses = {})
{
	RenderFrame frame{};
	DebugStatsServices stats{};
	BuildRenderFrame(world, WorldCameraSample{}, 0, 0, poses, stats, frame);
	return std::move(frame.objects);
}

[[nodiscard]] Result<void> TestSampleLeavesTransform()
{
	Result<PhysicsScene> created = CreateSimulation();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsScene& simulation = created.value;
	World world{};
	WorldSpawnDesc parentDesc{};
	parentDesc.transform.position = { 0.0f, 4.0f, 0.0f };
	const EntityId parent = world.Spawn(parentDesc);
	WorldSpawnDesc childDesc{};
	childDesc.parent = parent;
	childDesc.transform.position = { 0.0f, 1.0f, 0.0f };
	const EntityId child = world.Spawn(childDesc);
	Renderable childRenderable{};
	childRenderable.visible = true;
	if (!world.Add<Renderable>(child, std::move(childRenderable)))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Interpolation test should add a child renderable");
	}

	PhysicsBodyDesc sphere{};
	sphere.motion = PhysicsMotion::Dynamic;
	sphere.shape.type = PhysicsShapeType::Sphere;
	sphere.shape.radius = 0.5f;
	if (auto body = simulation.Bind(world, parent, sphere); !body)
	{
		return FailInternal(LogCategory::Physics, body.error.code, body.error.message);
	}

	constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
	for (int stepIndex = 0; stepIndex < 30; ++stepIndex)
	{
		if (auto step = simulation.Step(world, kFixedDeltaSeconds); !step)
		{
			return step;
		}
	}

	simulation.CommitTransforms(world);
	const TransformComponent* transform = world.GetTransform(parent);
	if (transform == nullptr)
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Committed body should keep its transform");
	}

	const float simulatedY = transform->position.y;
	std::vector<RenderPose> previous{};
	simulation.SamplePoses(0.0f, previous);
	const TransformComponent* afterSample = world.GetTransform(parent);
	if (previous.size() != 1 || afterSample == nullptr
		|| !IsNearlyEqual(afterSample->position.y, simulatedY, 0.0001f)
		|| !(std::fabs(previous[0].position.y - simulatedY) > 0.01f))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Sampling a previous pose should leave the committed transform unchanged");
	}

	std::vector<ExtractedObjectSnapshot> extracted = ExtractObjects(world, previous);
	const ExtractedObjectSnapshot* visualChild = FindExtracted(extracted, child);
	const bool foundVisual = visualChild != nullptr;
	const float visualY = foundVisual ? ExtractedTranslationY(*visualChild) : 0.0f;
	extracted = ExtractObjects(world);
	const ExtractedObjectSnapshot* simulatedChild = FindExtracted(extracted, child);
	const float simulatedChildY = simulatedChild == nullptr ? 0.0f : ExtractedTranslationY(*simulatedChild);
	const TransformComponent* afterExtract = world.GetTransform(parent);
	if (!foundVisual || simulatedChild == nullptr || afterExtract == nullptr
		|| !IsNearlyEqual(afterExtract->position.y, simulatedY, 0.0001f)
		|| !IsNearlyEqual(visualY, previous[0].position.y + 1.0f, 0.001f)
		|| !IsNearlyEqual(simulatedChildY, simulatedY + 1.0f, 0.001f))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"Extract should place a child on the sampled parent pose without writing the transform");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestKinematicFollowsTransform()
{
	Result<PhysicsScene> created = CreateSimulation();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsScene& simulation = created.value;
	World world{};
	WorldSpawnDesc spawn{};
	spawn.transform.position = { 0.0f, 3.0f, 0.0f };
	const EntityId entity = world.Spawn(spawn);

	PhysicsBodyDesc box{};
	box.motion = PhysicsMotion::Kinematic;
	box.shape.type = PhysicsShapeType::Box;
	box.shape.halfExtent = { 0.5f, 0.5f, 0.5f };
	if (auto body = simulation.Bind(world, entity, box); !body)
	{
		return FailInternal(LogCategory::Physics, body.error.code, body.error.message);
	}

	constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
	if (auto step = simulation.Step(world, kFixedDeltaSeconds); !step)
	{
		return step;
	}

	simulation.CommitTransforms(world);
	TransformComponent* transform = world.GetTransform(entity);
	if (transform == nullptr || !IsNearlyEqual(transform->position.y, 3.0f, 0.02f))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"A kinematic body should stay until its transform moves");
	}

	transform->position.y = 6.0f;
	if (auto step = simulation.Step(world, kFixedDeltaSeconds); !step)
	{
		return step;
	}

	simulation.CommitTransforms(world);
	const TransformComponent* committed = world.GetTransform(entity);
	if (committed == nullptr || !IsNearlyEqual(committed->position.y, 6.0f, 0.02f))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"A kinematic body should move to the transform written before the step");
	}

	const float movedY = committed->position.y;
	std::vector<RenderPose> previous{};
	simulation.SamplePoses(0.0f, previous);
	const TransformComponent* afterSample = world.GetTransform(entity);
	if (afterSample == nullptr || previous.size() != 1
		|| !IsNearlyEqual(afterSample->position.y, movedY, 0.0001f)
		|| !IsNearlyEqual(previous[0].position.y, 3.0f, 0.02f))
	{
		return FailInternal(LogCategory::Physics, ErrorCode::RuntimeError,
			"A kinematic body should move to the committed transform, and sampling should leave that transform in place");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestBindRejectsParentAndDuplicate()
{
	Result<PhysicsScene> created = CreateSimulation();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsScene& simulation = created.value;
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
	Result<PhysicsScene> created = CreateSimulation();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsScene& simulation = created.value;
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
	Result<PhysicsScene> created = CreateSimulation();
	if (!created)
	{
		return FailInternal(LogCategory::Physics, created.error.code, created.error.message);
	}

	PhysicsScene& simulation = created.value;
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

	if (auto sampled = TestSampleLeavesTransform(); !sampled)
	{
		return sampled;
	}

	if (auto kinematic = TestKinematicFollowsTransform(); !kinematic)
	{
		return kinematic;
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
