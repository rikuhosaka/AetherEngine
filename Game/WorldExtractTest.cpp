#include "Game/WorldExtractTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Math/Transform.h"
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

	WorldSpawnDesc parentDesc{};
	parentDesc.transform.position = { 2.0f, 0.0f, 0.0f };
	const DirectX::XMVECTOR yawRotation = DirectX::XMQuaternionRotationAxis(
		DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
		DirectX::XM_PIDIV2);
	DirectX::XMStoreFloat4(&parentDesc.transform.rotation, yawRotation);
	const EntityId parent = world.Spawn(std::move(parentDesc));

	WorldSpawnDesc childDesc{};
	childDesc.parent = parent;
	childDesc.transform.position = { 1.0f, 0.0f, 0.0f };
	childDesc.renderable.visible = true;
	const EntityId child = world.Spawn(std::move(childDesc));

	extracted.clear();
	ExtractWorld(world, extracted);
	const ExtractedObject* extractedChild = nullptr;
	for (const ExtractedObject& object : extracted)
	{
		if (object.objectId.Index == child.Index && object.objectId.Generation == child.Generation)
		{
			extractedChild = &object;
			break;
		}
	}

	if (extractedChild == nullptr)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Child entity should extract");
	}

	const DirectX::XMMATRIX parentMatrix = Aether::Math::TRS(
		DirectX::XMLoadFloat3(&world.GetTransform(parent)->position),
		DirectX::XMLoadFloat4(&world.GetTransform(parent)->rotation),
		DirectX::XMLoadFloat3(&world.GetTransform(parent)->scale));
	const DirectX::XMVECTOR expectedOffset = DirectX::XMVector3TransformCoord(
		DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f),
		parentMatrix);
	DirectX::XMFLOAT3 expectedOffsetStored{};
	DirectX::XMStoreFloat3(&expectedOffsetStored, expectedOffset);
	const DirectX::XMMATRIX extractedMatrix = Aether::Math::LoadMatrixFromHlsl(extractedChild->worldMatrix);
	DirectX::XMFLOAT4X4 extractedStored{};
	DirectX::XMStoreFloat4x4(&extractedStored, extractedMatrix);
	if (!IsNearlyEqual(extractedStored._41, expectedOffsetStored.x) ||
		!IsNearlyEqual(extractedStored._42, expectedOffsetStored.y) ||
		!IsNearlyEqual(extractedStored._43, expectedOffsetStored.z) ||
		IsNearlyEqual(extractedStored._41, 1.0f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extracted child matrix should apply the parent rotation to the local offset");
	}

	world.Destroy(parent);
	extracted.clear();
	ExtractWorld(world, extracted);
	for (const ExtractedObject& object : extracted)
	{
		if (object.objectId.Index == child.Index || object.objectId.Index == parent.Index)
		{
			return FailRuntime(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Destroyed parent and child should not extract");
		}
	}

	WorldSpawnDesc staleDesc{};
	staleDesc.parent = parent;
	staleDesc.transform.position = { 4.0f, 0.0f, 0.0f };
	const EntityId staleChild = world.Spawn(std::move(staleDesc));
	if (staleChild.Index != parent.Index)
	{
		for (int attempt = 0; attempt < 4; ++attempt)
		{
			const EntityId spawned = world.Spawn(WorldSpawnDesc{});
			if (spawned.Index == parent.Index)
			{
				world.GetTransform(spawned)->position = { 100.0f, 0.0f, 0.0f };
				break;
			}
		}
	}

	extracted.clear();
	ExtractWorld(world, extracted);
	const ExtractedObject* extractedStale = nullptr;
	for (const ExtractedObject& object : extracted)
	{
		if (object.objectId.Index == staleChild.Index && object.objectId.Generation == staleChild.Generation)
		{
			extractedStale = &object;
			break;
		}
	}

	if (extractedStale == nullptr)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Entity with a stale parent should still extract");
	}

	const DirectX::XMMATRIX staleMatrix = Aether::Math::LoadMatrixFromHlsl(extractedStale->worldMatrix);
	DirectX::XMFLOAT4X4 staleStored{};
	DirectX::XMStoreFloat4x4(&staleStored, staleMatrix);
	if (!IsNearlyEqual(staleStored._41, 4.0f) ||
		!IsNearlyEqual(staleStored._42, 0.0f) ||
		!IsNearlyEqual(staleStored._43, 0.0f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extract should treat a destroyed parent generation as a root");
	}

	LOG_INFO(LogCategory::ECS, "WorldExtractTests passed");
	return MakeOk();
}
