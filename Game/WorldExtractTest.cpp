#include "Game/WorldExtractTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/World/World.h"
#include "Game/WorldExtract.h"
#include "Game/WorldRenderBridge.h"

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
} // namespace

Result<void> RunWorldExtractTests()
{
	World world{};
	WorldSpawnDesc spawnDesc{};
	spawnDesc.transform.position = { 5.0f, 2.0f, -1.0f };
	spawnDesc.renderable.visible = true;
	spawnDesc.renderable.mesh = ToWorldMeshId(MeshHandle{ 3, 1 });
	spawnDesc.renderable.material = ToWorldMaterialId(MaterialHandle{ 4, 2 });
	spawnDesc.renderable.layerMask = WorldLayer::Opaque | WorldLayer::Shadow;

	const EntityId first = world.Spawn(std::move(spawnDesc));

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

	if (extracted[0].mesh.Index != 3 || extracted[0].mesh.Generation != 1 ||
		extracted[0].material.Index != 4 || extracted[0].material.Generation != 2 ||
		extracted[0].layerMask != (RenderLayer::Opaque | RenderLayer::Shadow))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extracted handles should match world ids");
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
	extracted.clear();
	ExtractWorld(world, extracted);
	if (!extracted.empty())
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Destroyed entity should not extract");
	}

	const EntityId second = world.Spawn(WorldSpawnDesc{});
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

	LOG_INFO(LogCategory::ECS, "WorldExtractTests passed");
	return MakeOk();
}
