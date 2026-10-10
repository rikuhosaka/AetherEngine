#include "Engine/World/Test/WorldTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Math/Transform.h"
#include "Engine/World/World.h"

#include <DirectXMath.h>

#include <cmath>
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

[[nodiscard]] bool IsNearlyEqual(float left, float right, float epsilon = 0.0001f)
{
	return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] DirectX::XMMATRIX ToMatrix(const Transform& transform)
{
	return Aether::Math::TRS(
		DirectX::XMLoadFloat3(&transform.position),
		DirectX::XMLoadFloat4(&transform.rotation),
		DirectX::XMLoadFloat3(&transform.scale));
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

	WorldSpawnDesc parentDesc{};
	parentDesc.transform.position = { 2.0f, 3.0f, 4.0f };
	const EntityId parent = world.Spawn(std::move(parentDesc));

	WorldSpawnDesc childDesc{};
	childDesc.parent = parent;
	childDesc.transform.position = { 1.0f, 0.0f, -2.0f };
	const EntityId child = world.Spawn(std::move(childDesc));
	if (world.GetParent(child) != parent)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Spawn should store the parent entity");
	}

	const DirectX::XMMATRIX childWorld = world.GetWorldMatrix(child);
	DirectX::XMFLOAT4X4 childWorldStored{};
	DirectX::XMStoreFloat4x4(&childWorldStored, childWorld);
	if (!IsNearlyEqual(childWorldStored._41, 3.0f) ||
		!IsNearlyEqual(childWorldStored._42, 3.0f) ||
		!IsNearlyEqual(childWorldStored._43, 2.0f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Child world translation should add the parent translation");
	}

	const Transform childLocalBefore = *world.GetTransform(child);
	if (!world.SetParent(child, {}))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"SetParent should detach a child when the parent is invalid");
	}

	if (world.GetParent(child).IsValid() ||
		world.GetTransform(child)->position.x != childLocalBefore.position.x)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Detaching a parent should keep the local transform");
	}

	if (!world.SetParent(child, parent) || world.SetParent(parent, child) || world.SetParent(parent, parent))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"SetParent should reject cycles and self parenting");
	}

	if (world.SetParent(child, first))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"SetParent should reject a dead parent");
	}

	WorldSpawnDesc grandchildDesc{};
	grandchildDesc.transform.position = { 0.0f, 1.0f, 0.0f };
	const EntityId grandchild = world.Spawn(std::move(grandchildDesc));
	if (!world.SetParent(grandchild, child))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"SetParent should attach a live child");
	}

	const DirectX::XMVECTOR yawRotation = DirectX::XMQuaternionRotationAxis(
		DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
		DirectX::XM_PIDIV2);
	DirectX::XMStoreFloat4(&world.GetTransform(parent)->rotation, yawRotation);
	const Transform* parentTransform = world.GetTransform(parent);
	const Transform* rotatedChild = world.GetTransform(child);
	const DirectX::XMMATRIX expectedChildWorld = DirectX::XMMatrixMultiply(
		ToMatrix(*rotatedChild),
		ToMatrix(*parentTransform));
	const DirectX::XMVECTOR expectedOffset = DirectX::XMVector3TransformCoord(
		DirectX::XMVectorSet(rotatedChild->position.x, rotatedChild->position.y, rotatedChild->position.z, 1.0f),
		ToMatrix(*parentTransform));
	DirectX::XMFLOAT3 expectedOffsetStored{};
	DirectX::XMStoreFloat3(&expectedOffsetStored, expectedOffset);
	DirectX::XMFLOAT4X4 rotatedStored{};
	DirectX::XMStoreFloat4x4(&rotatedStored, world.GetWorldMatrix(child));
	DirectX::XMFLOAT4X4 expectedStored{};
	DirectX::XMStoreFloat4x4(&expectedStored, expectedChildWorld);
	if (!IsNearlyEqual(rotatedStored._41, expectedStored._41) ||
		!IsNearlyEqual(rotatedStored._42, expectedStored._42) ||
		!IsNearlyEqual(rotatedStored._43, expectedStored._43) ||
		!IsNearlyEqual(rotatedStored._41, expectedOffsetStored.x) ||
		!IsNearlyEqual(rotatedStored._42, expectedOffsetStored.y) ||
		!IsNearlyEqual(rotatedStored._43, expectedOffsetStored.z) ||
		IsNearlyEqual(rotatedStored._41, rotatedChild->position.x))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Parent rotation should be applied to the child local offset");
	}

	world.Destroy(parent);
	if (world.IsAlive(parent) || world.IsAlive(child) || world.IsAlive(grandchild) || CountAlive(world) != 1)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Destroy should remove the parent and its descendants");
	}

	WorldSpawnDesc staleChildDesc{};
	staleChildDesc.parent = parent;
	staleChildDesc.transform.position = { 4.0f, 0.0f, 0.0f };
	const EntityId staleChild = world.Spawn(std::move(staleChildDesc));
	if (staleChild.Index != parent.Index)
	{
		bool reusedParentSlot = false;
		for (int attempt = 0; attempt < 4; ++attempt)
		{
			const EntityId spawned = world.Spawn(WorldSpawnDesc{});
			if (spawned.Index != parent.Index)
			{
				continue;
			}

			world.GetTransform(spawned)->position = { 100.0f, 0.0f, 0.0f };
			reusedParentSlot = true;
			break;
		}

		if (!reusedParentSlot)
		{
			return FailRuntime(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Stale parent test requires the destroyed parent slot");
		}
	}

	DirectX::XMFLOAT4X4 staleStored{};
	DirectX::XMStoreFloat4x4(&staleStored, world.GetWorldMatrix(staleChild));
	if (!IsNearlyEqual(staleStored._41, 4.0f) ||
		!IsNearlyEqual(staleStored._42, 0.0f) ||
		!IsNearlyEqual(staleStored._43, 0.0f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"A destroyed parent generation should be treated as a root");
	}

	LOG_INFO(LogCategory::ECS, "WorldTests passed");
	return MakeOk();
}
