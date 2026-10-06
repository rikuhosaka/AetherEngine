#include "Engine/World/Test/WorldTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/World/World.h"
#include "Engine/World/WorldExtract.h"

#include <DirectXMath.h>

#include <cmath>
#include <utility>
#include <vector>

namespace
{
[[nodiscard]] bool IsNearlyEqual(float left, float right, float epsilon = 0.0001f)
{
	return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] Result<void> ExpectExtractCount(
	const World& world,
	size_t expectedCount,
	const char* message)
{
	std::vector<ExtractedObject> extracted;
	ExtractWorld(world, extracted);
	if (extracted.size() != expectedCount)
	{
		return FailRuntime(LogCategory::ECS, ErrorCode::InvalidArgument, message);
	}

	return MakeOk();
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

	std::vector<ExtractedObject> extracted;
	ExtractWorld(world, extracted);
	if (extracted.size() != 1)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Spawned entity should extract as one object");
	}

	if (extracted[0].objectId.Index != first.Index || extracted[0].objectId.Generation != first.Generation)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extracted object id should match entity id");
	}

	const DirectX::XMMATRIX worldMatrix = Aether::Math::LoadMatrixFromHlsl(extracted[0].worldMatrix);
	DirectX::XMFLOAT4X4 stored{};
	DirectX::XMStoreFloat4x4(&stored, worldMatrix);
	if (!IsNearlyEqual(stored._41, 5.0f) ||
		!IsNearlyEqual(stored._42, 2.0f) ||
		!IsNearlyEqual(stored._43, -1.0f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extracted world matrix translation is incorrect");
	}

	world.Destroy(first);
	if (world.IsAlive(first) || world.GetTransform(first) != nullptr)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Destroyed entity should be invalid");
	}

	if (auto destroyExtract = ExpectExtractCount(world, 0, "Destroyed entity should not extract");
		!destroyExtract)
	{
		return destroyExtract;
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

	extracted.clear();
	ExtractWorld(world, extracted);
	if (extracted.size() != 1 ||
		extracted[0].objectId.Index != second.Index ||
		extracted[0].objectId.Generation != second.Generation)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extract should use the reused entity generation");
	}

	LOG_INFO(LogCategory::ECS, "WorldTests passed");
	return MakeOk();
}
