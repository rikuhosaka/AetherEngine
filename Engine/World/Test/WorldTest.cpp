#include "Engine/World/Test/WorldTest.h"

#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Services/DebugStatsServices.h"
#include "Engine/Application/Services/EditorCameraServices.h"
#include "Engine/Application/Services/SceneContentServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Camera/CameraComponent.h"
#include "Engine/Camera/CameraExtract.h"
#include "Engine/Camera/CameraState.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Frame/FrameContext.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Math/Transform.h"
#include "Engine/Platform/InputState.h"
#include "Engine/Renderer/Scene/RenderLightingTypes.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/Renderer/Scene/RenderViewTypes.h"
#include "Engine/World/RigidBody.h"
#include "Engine/World/World.h"
#include "Engine/World/WorldDocument.h"

#include <DirectXMath.h>

#include <cmath>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
[[nodiscard]] size_t CountAlive(const World& world)
{
	size_t count = 0;
	world.ForEachAlive([&count](EntityId, const Transform&) {
		++count;
	});
	return count;
}

[[nodiscard]] std::vector<EntityId> CollectRoots(const World& world)
{
	std::vector<EntityId> ids;
	world.ForEachRoot([&ids](EntityId id) {
		ids.push_back(id);
	});
	return ids;
}

[[nodiscard]] std::vector<EntityId> CollectChildren(const World& world, EntityId parent)
{
	std::vector<EntityId> ids;
	world.ForEachChild(parent, [&ids](EntityId id) {
		ids.push_back(id);
	});
	return ids;
}

[[nodiscard]] bool ContainsEntity(const std::vector<EntityId>& ids, EntityId id)
{
	for (const EntityId entry : ids)
	{
		if (entry == id)
		{
			return true;
		}
	}

	return false;
}

[[nodiscard]] bool IsNearlyEqual(float left, float right, float epsilon = 0.0001f)
{
	return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] DirectX::XMFLOAT3 ForwardFromRotation(const DirectX::XMFLOAT4& rotation)
{
	using namespace DirectX;

	const XMVECTOR forward = XMVector3Rotate(
		XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
		XMLoadFloat4(&rotation));
	XMFLOAT3 stored{};
	XMStoreFloat3(&stored, forward);
	return stored;
}

[[nodiscard]] bool SameExtractedView(const ExtractedView& left, const ExtractedView& right)
{
	if (left.viewportWidth != right.viewportWidth || left.viewportHeight != right.viewportHeight)
	{
		return false;
	}

	for (int index = 0; index < 3; ++index)
	{
		if (!IsNearlyEqual(left.cameraPosition[index], right.cameraPosition[index]))
		{
			return false;
		}
	}

	for (int index = 0; index < 16; ++index)
	{
		if (!IsNearlyEqual(left.viewMatrix[index], right.viewMatrix[index])
			|| !IsNearlyEqual(left.projectionMatrix[index], right.projectionMatrix[index])
			|| !IsNearlyEqual(left.viewProjectionMatrix[index], right.viewProjectionMatrix[index]))
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] DirectX::XMMATRIX ToMatrix(const Transform& transform)
{
	return Aether::Math::TRS(
		DirectX::XMLoadFloat3(&transform.position),
		DirectX::XMLoadFloat4(&transform.rotation),
		DirectX::XMLoadFloat3(&transform.scale));
}

class RecordingLoader final : public ISceneContent
{
public:
	Result<void> Load(
		World& world,
		SubsystemContext&,
		FrameContext&,
		RHICommandList*) override
	{
		WorldSpawnDesc desc{};
		desc.name = "Loaded";
		spawned = world.Spawn(std::move(desc));
		return MakeOk();
	}

	EntityId spawned{};
};

[[nodiscard]] float ExtractedTranslationY(const ExtractedObject& object)
{
	DirectX::XMFLOAT4X4 decoded{};
	DirectX::XMStoreFloat4x4(&decoded, Aether::Math::LoadMatrixFromHlsl(object.worldMatrix));
	return decoded._42;
}

[[nodiscard]] bool HasNamedEntity(const World& world, std::string_view name)
{
	bool found = false;
	world.ForEachAlive([&world, &found, name](EntityId id, const Transform&) {
		if (world.GetName(id) == name)
		{
			found = true;
		}
	});
	return found;
}

[[nodiscard]] bool SameDocumentEntity(const WorldDocumentEntity& left, const WorldDocumentEntity& right)
{
	return left.id == right.id && left.parent == right.parent && left.name == right.name
		&& IsNearlyEqual(left.transform.position.x, right.transform.position.x)
		&& IsNearlyEqual(left.transform.position.y, right.transform.position.y)
		&& IsNearlyEqual(left.transform.position.z, right.transform.position.z)
		&& IsNearlyEqual(left.transform.rotation.x, right.transform.rotation.x)
		&& IsNearlyEqual(left.transform.rotation.y, right.transform.rotation.y)
		&& IsNearlyEqual(left.transform.rotation.z, right.transform.rotation.z)
		&& IsNearlyEqual(left.transform.rotation.w, right.transform.rotation.w)
		&& IsNearlyEqual(left.transform.scale.x, right.transform.scale.x)
		&& IsNearlyEqual(left.transform.scale.y, right.transform.scale.y)
		&& IsNearlyEqual(left.transform.scale.z, right.transform.scale.z)
		&& left.submeshIndex == right.submeshIndex && left.layerMask == right.layerMask
		&& left.visible == right.visible;
}
} // namespace

Result<void> RunWorldTests()
{
	World world{};
	WorldSpawnDesc spawnDesc{};
	spawnDesc.name = "First";
	spawnDesc.transform.position = { 5.0f, 2.0f, -1.0f };

	const EntityId first = world.Spawn(std::move(spawnDesc));
	Renderable firstRenderable{};
	firstRenderable.visible = true;
	if (!world.IsAlive(first) || world.GetTransform(first) == nullptr || !world.Add<Renderable>(first, std::move(firstRenderable))
		|| world.Get<Renderable>(first) == nullptr)
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

	if (world.GetName(first) != "First")
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Spawn should store the entity display name");
	}

	Renderable extraRenderable{};
	if (world.Add<Renderable>(first, std::move(extraRenderable)) || !world.Get<Renderable>(first)->visible || CountAlive(world) != 1)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Spawn should store a visible renderable");
	}

	world.Destroy(first);
	if (world.IsAlive(first) || world.GetTransform(first) != nullptr || !world.GetName(first).empty()
		|| CountAlive(world) != 0)
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

	if (!world.GetName(second).empty())
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Reused slot should not keep the previous display name");
	}

	bool sawSecond = false;
	world.ForEachAlive([&](EntityId id, const Transform&) {
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

	const std::vector<EntityId> roots = CollectRoots(world);
	const std::vector<EntityId> children = CollectChildren(world, parent);
	if (roots.size() != 2 || !ContainsEntity(roots, second) || !ContainsEntity(roots, parent)
		|| ContainsEntity(roots, child) || children.size() != 1 || children.front() != child
		|| !CollectChildren(world, child).empty() || !CollectChildren(world, first).empty())
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Hierarchy reads should list roots and direct children");
	}

	WorldSpawnDesc siblingDesc{};
	siblingDesc.parent = parent;
	const EntityId sibling = world.Spawn(std::move(siblingDesc));
	const std::vector<EntityId> siblings = CollectChildren(world, parent);
	if (siblings.size() != 2 || siblings[0] != child || siblings[1] != sibling)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Direct children should be listed in slot order");
	}

	world.Destroy(sibling);

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
		world.GetTransform(child)->position.x != childLocalBefore.position.x ||
		!CollectChildren(world, parent).empty() || !ContainsEntity(CollectRoots(world), child))
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

	const std::vector<EntityId> parentChildren = CollectChildren(world, parent);
	const std::vector<EntityId> childChildren = CollectChildren(world, child);
	if (parentChildren.size() != 1 || parentChildren.front() != child || childChildren.size() != 1
		|| childChildren.front() != grandchild || ContainsEntity(parentChildren, grandchild))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Hierarchy reads should not skip to grandchildren");
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
	EntityId reusedParentSlot{};
	if (staleChild.Index != parent.Index)
	{
		bool foundReusedParentSlot = false;
		for (int attempt = 0; attempt < 4; ++attempt)
		{
			const EntityId spawned = world.Spawn(WorldSpawnDesc{});
			if (spawned.Index != parent.Index)
			{
				continue;
			}

			world.GetTransform(spawned)->position = { 100.0f, 0.0f, 0.0f };
			reusedParentSlot = spawned;
			foundReusedParentSlot = true;
			break;
		}

		if (!foundReusedParentSlot)
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
		!IsNearlyEqual(staleStored._43, 0.0f) ||
		!ContainsEntity(CollectRoots(world), staleChild) ||
		!CollectChildren(world, parent).empty() ||
		(reusedParentSlot.IsValid() && ContainsEntity(CollectChildren(world, reusedParentSlot), staleChild)))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"A destroyed parent generation should be treated as a root");
	}

	World cloneSource{};
	WorldSpawnDesc cloneRootDesc{};
	cloneRootDesc.name = "Root";
	cloneRootDesc.transform.position = { 1.0f, 2.0f, 3.0f };
	const EntityId cloneRoot = cloneSource.Spawn(std::move(cloneRootDesc));
	Renderable cloneRootRenderable{};
	cloneRootRenderable.mesh.Index = 7;
	cloneRootRenderable.mesh.Generation = 1;
	cloneRootRenderable.visible = false;
	if (!cloneSource.Add<Renderable>(cloneRoot, std::move(cloneRootRenderable)))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Clone source should accept a renderable");
	}

	WorldSpawnDesc cloneChildDesc{};
	cloneChildDesc.name = "Child";
	cloneChildDesc.parent = cloneRoot;
	cloneChildDesc.transform.position = { 0.5f, 0.0f, -2.0f };
	const EntityId cloneChild = cloneSource.Spawn(std::move(cloneChildDesc));
	Renderable cloneChildRenderable{};
	cloneChildRenderable.material.Index = 3;
	cloneChildRenderable.material.Generation = 2;
	if (!cloneSource.Add<Renderable>(cloneChild, std::move(cloneChildRenderable)))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Clone source should accept a child renderable");
	}

	WorldSpawnDesc cloneDoomedDesc{};
	cloneDoomedDesc.name = "Doomed";
	const EntityId cloneDoomed = cloneSource.Spawn(std::move(cloneDoomedDesc));
	cloneSource.Destroy(cloneDoomed);

	LightingSettings& sourceLighting = cloneSource.GetLighting();
	sourceLighting.ambientColor[0] = 0.25f;
	sourceLighting.ambientColor[1] = 0.5f;
	sourceLighting.ambientColor[2] = 0.75f;
	sourceLighting.ambientIntensity = 0.4f;
	sourceLighting.mainLightColor[0] = 0.1f;
	sourceLighting.mainLightColor[1] = 0.2f;
	sourceLighting.mainLightColor[2] = 0.3f;
	sourceLighting.mainLightIntensity = 2.5f;
	sourceLighting.mainLightDirection[0] = 0.4f;
	sourceLighting.mainLightDirection[1] = -0.5f;
	sourceLighting.mainLightDirection[2] = 0.6f;

	World clone = cloneSource.Clone();
	const Transform* clonedRootTransform = clone.GetTransform(cloneRoot);
	const Transform* clonedChildTransform = clone.GetTransform(cloneChild);
	const Renderable* clonedRootRenderable = clone.Get<Renderable>(cloneRoot);
	const Renderable* clonedChildRenderable = clone.Get<Renderable>(cloneChild);
	const LightingSettings& clonedLighting = clone.GetLighting();
	if (!clone.IsAlive(cloneRoot) || !clone.IsAlive(cloneChild) || clone.IsAlive(cloneDoomed) ||
		clone.GetName(cloneRoot) != "Root" || clone.GetName(cloneChild) != "Child" ||
		clone.GetParent(cloneChild) != cloneRoot || clone.GetParent(cloneRoot).IsValid() ||
		clonedRootTransform == nullptr || clonedChildTransform == nullptr ||
		!IsNearlyEqual(clonedRootTransform->position.x, 1.0f) ||
		!IsNearlyEqual(clonedRootTransform->position.y, 2.0f) ||
		!IsNearlyEqual(clonedRootTransform->position.z, 3.0f) ||
		!IsNearlyEqual(clonedChildTransform->position.x, 0.5f) ||
		!IsNearlyEqual(clonedChildTransform->position.z, -2.0f) ||
		clonedRootRenderable == nullptr || clonedChildRenderable == nullptr ||
		clonedRootRenderable->mesh.Index != 7 || clonedRootRenderable->mesh.Generation != 1 ||
		clonedRootRenderable->visible ||
		clonedChildRenderable->material.Index != 3 || clonedChildRenderable->material.Generation != 2 ||
		!IsNearlyEqual(clonedLighting.ambientColor[0], 0.25f) ||
		!IsNearlyEqual(clonedLighting.ambientColor[1], 0.5f) ||
		!IsNearlyEqual(clonedLighting.ambientColor[2], 0.75f) ||
		!IsNearlyEqual(clonedLighting.ambientIntensity, 0.4f) ||
		!IsNearlyEqual(clonedLighting.mainLightColor[0], 0.1f) ||
		!IsNearlyEqual(clonedLighting.mainLightColor[1], 0.2f) ||
		!IsNearlyEqual(clonedLighting.mainLightColor[2], 0.3f) ||
		!IsNearlyEqual(clonedLighting.mainLightIntensity, 2.5f) ||
		!IsNearlyEqual(clonedLighting.mainLightDirection[0], 0.4f) ||
		!IsNearlyEqual(clonedLighting.mainLightDirection[1], -0.5f) ||
		!IsNearlyEqual(clonedLighting.mainLightDirection[2], 0.6f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Clone should copy live entities, dead generations, parents, transforms, renderables, and lighting");
	}

	const EntityId sourceReuse = cloneSource.Spawn(WorldSpawnDesc{});
	const EntityId cloneReuse = clone.Spawn(WorldSpawnDesc{});
	if (sourceReuse != cloneReuse || sourceReuse.Index != cloneDoomed.Index ||
		sourceReuse.Generation != cloneDoomed.Generation + 1)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Clone should reuse the same free slot as the source");
	}

	clone.GetTransform(cloneRoot)->position = { 9.0f, 9.0f, 9.0f };
	clone.GetLighting().ambientIntensity = 9.0f;
	const Transform* sourceRootTransform = cloneSource.GetTransform(cloneRoot);
	if (sourceRootTransform == nullptr ||
		!IsNearlyEqual(sourceRootTransform->position.x, 1.0f) ||
		!IsNearlyEqual(sourceRootTransform->position.y, 2.0f) ||
		!IsNearlyEqual(sourceRootTransform->position.z, 3.0f) ||
		!IsNearlyEqual(cloneSource.GetLighting().ambientIntensity, 0.4f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Changing a clone transform or lighting should leave the source world unchanged");
	}

	World documentWorld{};
	WorldSpawnDesc existingDesc{};
	existingDesc.name = "Existing";
	const EntityId existing = documentWorld.Spawn(std::move(existingDesc));

	WorldDocument empty{};
	const Result<std::vector<WorldDocumentBinding>> emptyResult = Instantiate(documentWorld, empty);
	if (!emptyResult || !emptyResult.value.empty() || CountAlive(documentWorld) != 1
		|| !documentWorld.IsAlive(existing) || documentWorld.GetName(existing) != "Existing")
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"An empty world document should leave the destination world unchanged");
	}

	WorldDocument document{};
	WorldDocumentEntity childEntity{};
	childEntity.id = 2;
	childEntity.parent = 1;
	childEntity.name = "Child";
	childEntity.transform.position = { 1.0f, 2.0f, 3.0f };
	childEntity.visible = false;
	document.push_back(childEntity);

	WorldDocumentEntity parentEntity{};
	parentEntity.id = 1;
	parentEntity.name = "Parent";
	parentEntity.transform.position = { 10.0f, 0.0f, 0.0f };
	parentEntity.submeshIndex = 2;
	parentEntity.layerMask = WorldLayer::Opaque | WorldLayer::Shadow;
	parentEntity.visible = true;
	document.push_back(parentEntity);

	const Result<std::vector<WorldDocumentBinding>> instantiated = Instantiate(documentWorld, document);
	EntityId documentParent{};
	EntityId documentChild{};
	if (instantiated)
	{
		for (const WorldDocumentBinding& binding : instantiated.value)
		{
			if (binding.id == 1)
			{
				documentParent = binding.entity;
			}
			else if (binding.id == 2)
			{
				documentChild = binding.entity;
			}
		}
	}

	const Transform* documentParentTransform = documentWorld.GetTransform(documentParent);
	const Transform* documentChildTransform = documentWorld.GetTransform(documentChild);
	const Renderable* documentParentRenderable = documentWorld.Get<Renderable>(documentParent);
	const Renderable* documentChildRenderable = documentWorld.Get<Renderable>(documentChild);
	DirectX::XMFLOAT4X4 documentChildWorld{};
	DirectX::XMStoreFloat4x4(&documentChildWorld, documentWorld.GetWorldMatrix(documentChild));
	if (!instantiated || instantiated.value.size() != 2 || !documentWorld.IsAlive(documentParent)
		|| !documentWorld.IsAlive(documentChild) || documentWorld.GetParent(documentChild) != documentParent
		|| documentWorld.GetParent(documentParent).IsValid() || documentWorld.GetName(documentParent) != "Parent"
		|| documentWorld.GetName(documentChild) != "Child" || documentParentTransform == nullptr
		|| documentChildTransform == nullptr || !IsNearlyEqual(documentParentTransform->position.x, 10.0f)
		|| !IsNearlyEqual(documentChildTransform->position.x, 1.0f)
		|| !IsNearlyEqual(documentChildTransform->position.y, 2.0f)
		|| !IsNearlyEqual(documentChildTransform->position.z, 3.0f)
		|| !IsNearlyEqual(documentChildWorld._41, 11.0f) || !IsNearlyEqual(documentChildWorld._42, 2.0f)
		|| !IsNearlyEqual(documentChildWorld._43, 3.0f) || documentParentRenderable == nullptr
		|| documentChildRenderable == nullptr || !documentParentRenderable->visible || documentChildRenderable->visible
		|| documentParentRenderable->submeshIndex != 2
		|| documentParentRenderable->layerMask != (WorldLayer::Opaque | WorldLayer::Shadow)
		|| documentParentRenderable->mesh.IsValid() || documentParentRenderable->material.IsValid()
		|| CountAlive(documentWorld) != 3)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Instantiate should restore hierarchy, transforms, names, and visibility");
	}

	const size_t aliveBeforeReject = CountAlive(documentWorld);
	WorldDocument duplicate{};
	duplicate.push_back(parentEntity);
	duplicate.push_back(parentEntity);
	const Result<std::vector<WorldDocumentBinding>> duplicateResult = Instantiate(documentWorld, duplicate);
	if (duplicateResult || CountAlive(documentWorld) != aliveBeforeReject || !documentWorld.IsAlive(existing))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"A duplicated world document id should add no entities");
	}

	WorldDocument missing{};
	WorldDocumentEntity orphan{};
	orphan.id = 4;
	orphan.parent = 9;
	missing.push_back(orphan);
	const Result<std::vector<WorldDocumentBinding>> missingResult = Instantiate(documentWorld, missing);
	if (missingResult || CountAlive(documentWorld) != aliveBeforeReject || !documentWorld.IsAlive(existing))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"A missing world document parent should add no entities");
	}

	WorldDocument cycle{};
	WorldDocumentEntity cycleA{};
	cycleA.id = 5;
	cycleA.parent = 6;
	WorldDocumentEntity cycleB{};
	cycleB.id = 6;
	cycleB.parent = 5;
	cycle.push_back(cycleA);
	cycle.push_back(cycleB);
	const Result<std::vector<WorldDocumentBinding>> cycleResult = Instantiate(documentWorld, cycle);
	if (cycleResult || CountAlive(documentWorld) != aliveBeforeReject || !documentWorld.IsAlive(existing))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"A cyclic world document parent should add no entities");
	}

	WorldDocument encoded{};
	WorldDocumentEntity encodedChild = childEntity;
	encodedChild.name = "Child \"A\"";
	encodedChild.transform.rotation = { 0.1f, 0.2f, 0.3f, 0.9f };
	encodedChild.transform.scale = { 2.0f, 3.0f, 4.0f };
	encoded.push_back(encodedChild);
	encoded.push_back(parentEntity);

	const Result<std::string> written = WriteWorldDocument(encoded);
	const Result<WorldDocument> readBack = written ? ReadWorldDocument(written.value) : MakeFail<WorldDocument>(written.error.code, written.error.message);
	const Result<std::string> writtenAgain = readBack ? WriteWorldDocument(readBack.value) : MakeFail<std::string>(readBack.error.code, readBack.error.message);
	const Result<WorldDocument> readAgain = writtenAgain ? ReadWorldDocument(writtenAgain.value) : MakeFail<WorldDocument>(writtenAgain.error.code, writtenAgain.error.message);
	const bool roundTripMatches = readAgain && readAgain.value.size() == encoded.size()
		&& SameDocumentEntity(readAgain.value[0], encoded[0])
		&& SameDocumentEntity(readAgain.value[1], encoded[1]);
	if (!written || !readBack || !writtenAgain || !roundTripMatches)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"World document JSON should round-trip entities");
	}

	const Result<std::string> emptyWritten = WriteWorldDocument(WorldDocument{});
	const Result<WorldDocument> emptyRead = emptyWritten ? ReadWorldDocument(emptyWritten.value) : MakeFail<WorldDocument>(emptyWritten.error.code, emptyWritten.error.message);
	const Result<WorldDocument> invalidJson = ReadWorldDocument("{");
	const Result<WorldDocument> unsupportedVersion = ReadWorldDocument("{\"version\":2,\"entities\":[]}");
	const Result<WorldDocument> missingField = ReadWorldDocument("{\"version\":1,\"entities\":[{\"id\":1}]}");
	if (!emptyWritten || !emptyRead || !emptyRead.value.empty() || invalidJson || unsupportedVersion || missingField)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"World document JSON should reject an unsupported version, invalid text, and a missing field");
	}

	World bare{};
	const EntityId bareEntity = bare.Spawn(WorldSpawnDesc{});
	if (!bare.IsAlive(bareEntity) || bare.Get<Renderable>(bareEntity) != nullptr)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Spawn without a renderable should leave that component empty");
	}

	Renderable marked{};
	marked.mesh.Index = 4;
	marked.mesh.Generation = 2;
	WorldSpawnDesc markedDesc{};
	const EntityId markedEntity = bare.Spawn(std::move(markedDesc));
	if (!bare.Add<Renderable>(markedEntity, std::move(marked)))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Add should store a renderable");
	}
	bare.Destroy(markedEntity);
	const EntityId reusedBare = bare.Spawn(WorldSpawnDesc{});
	if (reusedBare.Index != markedEntity.Index || reusedBare.Generation != markedEntity.Generation + 1
		|| bare.Get<Renderable>(reusedBare) != nullptr)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Reused slot should not keep the previous renderable");
	}

	World renderSource{};
	Renderable copied{};
	copied.mesh.Index = 9;
	copied.mesh.Generation = 1;
	copied.visible = false;
	WorldSpawnDesc copiedDesc{};
	const EntityId copiedEntity = renderSource.Spawn(std::move(copiedDesc));
	if (!renderSource.Add<Renderable>(copiedEntity, std::move(copied)))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Add should store a renderable");
	}
	const EntityId emptyEntity = renderSource.Spawn(WorldSpawnDesc{});
	World renderClone = renderSource.Clone();
	const Renderable* clonedRenderable = renderClone.Get<Renderable>(copiedEntity);
	if (renderClone.Get<Renderable>(emptyEntity) != nullptr || clonedRenderable == nullptr
		|| clonedRenderable->mesh.Index != 9 || clonedRenderable->mesh.Generation != 1
		|| clonedRenderable->visible)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Clone should copy renderable presence and contents");
	}

	renderClone.Get<Renderable>(copiedEntity)->mesh.Index = 1;
	const Renderable* sourceRenderable = renderSource.Get<Renderable>(copiedEntity);
	if (sourceRenderable == nullptr || sourceRenderable->mesh.Index != 9)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Changing a cloned renderable should leave the source world unchanged");
	}

	World cameraWorld{};
	if (cameraWorld.FindCamera().IsValid())
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"FindCamera should miss when the world has no camera");
	}

	const EntityId spawnedCamera = cameraWorld.SpawnCamera(nullptr, nullptr);
	const Transform* spawnedTransform = cameraWorld.GetTransform(spawnedCamera);
	const CameraComponent* spawnedLens = cameraWorld.Get<CameraComponent>(spawnedCamera);
	const DirectX::XMFLOAT3 spawnedForward = spawnedTransform != nullptr
		? ForwardFromRotation(spawnedTransform->rotation)
		: DirectX::XMFLOAT3{};
	if (cameraWorld.FindCamera() != spawnedCamera || spawnedTransform == nullptr || spawnedLens == nullptr
		|| cameraWorld.GetName(spawnedCamera) != "Camera" || cameraWorld.GetParent(spawnedCamera).IsValid()
		|| !IsNearlyEqual(spawnedTransform->position.x, 0.0f)
		|| !IsNearlyEqual(spawnedTransform->position.y, 1.6f)
		|| !IsNearlyEqual(spawnedTransform->position.z, -4.0f)
		|| !IsNearlyEqual(spawnedForward.x, 0.0f)
		|| !IsNearlyEqual(spawnedForward.y, 0.0f)
		|| !IsNearlyEqual(spawnedForward.z, 1.0f)
		|| !IsNearlyEqual(spawnedLens->fovYDegrees, 60.0f)
		|| !IsNearlyEqual(spawnedLens->nearPlane, 0.1f)
		|| !IsNearlyEqual(spawnedLens->farPlane, 2000.0f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"SpawnCamera should create a root camera at the play fallback pose");
	}

	const DirectX::XMFLOAT3 posedPosition{ 4.0f, 5.0f, 6.0f };
	const DirectX::XMFLOAT3 posedForward{ 1.0f, 0.0f, 0.0f };
	const EntityId posedCamera = cameraWorld.SpawnCamera(&posedPosition, &posedForward);
	const Transform* posedTransform = cameraWorld.GetTransform(posedCamera);
	const DirectX::XMFLOAT3 posedAxis = posedTransform != nullptr
		? ForwardFromRotation(posedTransform->rotation)
		: DirectX::XMFLOAT3{};
	if (cameraWorld.FindCamera() != spawnedCamera || posedTransform == nullptr
		|| !IsNearlyEqual(posedTransform->position.x, 4.0f)
		|| !IsNearlyEqual(posedTransform->position.y, 5.0f)
		|| !IsNearlyEqual(posedTransform->position.z, 6.0f)
		|| !IsNearlyEqual(posedAxis.x, 1.0f)
		|| !IsNearlyEqual(posedAxis.y, 0.0f)
		|| !IsNearlyEqual(posedAxis.z, 0.0f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"SpawnCamera should use an explicit pose, and FindCamera should keep the earliest camera");
	}

	const InputState idleInput{};
	if (cameraWorld.UpdateCamera(spawnedCamera, nullptr, 0.1f, false, false)
		|| cameraWorld.UpdateCamera(spawnedCamera, &idleInput, 0.1f, false, false)
		|| !IsNearlyEqual(cameraWorld.GetTransform(spawnedCamera)->position.y, 1.6f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"UpdateCamera should leave the camera pose unchanged without input");
	}

	const EntityId plainEntity = cameraWorld.Spawn(WorldSpawnDesc{});
	if (cameraWorld.UpdateCamera(plainEntity, &idleInput, 0.1f, false, false))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"UpdateCamera should ignore an entity without a camera");
	}

	CameraState expectedPose{};
	expectedPose.position = { 0.0f, 1.6f, -4.0f };
	expectedPose.forward = { 0.0f, 0.0f, 1.0f };
	expectedPose.up = { 0.0f, 1.0f, 0.0f };
	ExtractedView expectedView{};
	BuildExtractedView(expectedPose, 640, 480, expectedView);
	ExtractedView extractedView{};
	cameraWorld.ExtractCameraView(spawnedCamera, 640, 480, extractedView);
	ExtractedView clearedView = expectedView;
	cameraWorld.ExtractCameraView(plainEntity, 640, 480, clearedView);
	if (!SameExtractedView(extractedView, expectedView) || clearedView.viewportWidth != 0
		|| clearedView.viewportHeight != 0)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"ExtractCameraView should match the camera world pose and clear a missing camera");
	}

	WorldSpawnDesc cameraParentDesc{};
	cameraParentDesc.transform.position = { 10.0f, 0.0f, 0.0f };
	const EntityId cameraParent = cameraWorld.Spawn(std::move(cameraParentDesc));
	if (!cameraWorld.SetParent(spawnedCamera, cameraParent))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"A camera should accept a parent");
	}

	expectedPose.position = { 10.0f, 1.6f, -4.0f };
	BuildExtractedView(expectedPose, 640, 480, expectedView);
	cameraWorld.Get<CameraComponent>(spawnedCamera)->fovYDegrees = 90.0f;
	cameraWorld.Get<CameraComponent>(spawnedCamera)->nearPlane = 1.0f;
	cameraWorld.Get<CameraComponent>(spawnedCamera)->farPlane = 100.0f;
	expectedPose.lens.fovYDegrees = 90.0f;
	expectedPose.lens.nearPlane = 1.0f;
	expectedPose.lens.farPlane = 100.0f;
	ExtractedView defaultLensView = expectedView;
	BuildExtractedView(expectedPose, 640, 480, expectedView);
	cameraWorld.ExtractCameraView(spawnedCamera, 640, 480, extractedView);
	if (!SameExtractedView(extractedView, expectedView) || SameExtractedView(extractedView, defaultLensView))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"ExtractCameraView should use the parented world pose and the camera lens");
	}

	World cameraClone = cameraWorld.Clone();
	const Transform* clonedCameraTransform = cameraClone.GetTransform(spawnedCamera);
	if (cameraClone.FindCamera() != spawnedCamera || clonedCameraTransform == nullptr
		|| !IsNearlyEqual(clonedCameraTransform->position.y, 1.6f)
		|| cameraClone.Get<CameraComponent>(spawnedCamera) == nullptr
		|| !IsNearlyEqual(cameraClone.Get<CameraComponent>(spawnedCamera)->fovYDegrees, 90.0f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Clone should copy the camera entity and its lens");
	}

	World sessionWorld{};
	WorldSpawnDesc sessionBodyDesc{};
	sessionBodyDesc.name = "Body";
	sessionBodyDesc.transform.position = { 0.0f, 4.0f, 0.0f };
	sessionBodyDesc.transform.scale = { 2.0f, 3.0f, 4.0f };
	const EntityId sessionBody = sessionWorld.Spawn(std::move(sessionBodyDesc));
	if (!sessionWorld.Add<RigidBody>(sessionBody, RigidBody{})
		|| !sessionWorld.Add<Renderable>(sessionBody, Renderable{}))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Play session should accept a rigid body and a renderable");
	}

	if (sessionWorld.BeginPlay(false, nullptr, nullptr) || sessionWorld.IsPlaying()
		|| !IsNearlyEqual(sessionWorld.GetTransform(sessionBody)->position.y, 4.0f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Play should require loaded scene content");
	}

	const DirectX::XMFLOAT3 playCameraPosition{ 2.0f, 3.0f, 4.0f };
	const DirectX::XMFLOAT3 playCameraForward{ 0.0f, 0.0f, 1.0f };
	if (auto begun = sessionWorld.BeginPlay(true, &playCameraPosition, &playCameraForward); !begun)
	{
		return begun;
	}

	const EntityId playCamera = sessionWorld.FindCamera();
	const Transform* playCameraTransform = sessionWorld.GetTransform(playCamera);
	if (!sessionWorld.IsPlaying() || playCamera == sessionBody || playCameraTransform == nullptr
		|| !IsNearlyEqual(playCameraTransform->position.x, 2.0f)
		|| !IsNearlyEqual(playCameraTransform->position.y, 3.0f)
		|| !IsNearlyEqual(playCameraTransform->position.z, 4.0f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Play should spawn a camera from the editor pose");
	}

	if (auto again = sessionWorld.BeginPlay(true, nullptr, nullptr); !again || sessionWorld.FindCamera() != playCamera)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Beginning play twice should keep the open session");
	}

	SessionServices playingSession{};
	playingSession.mode = SessionMode::Playing;
	constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;
	for (int stepIndex = 0; stepIndex < 30; ++stepIndex)
	{
		sessionWorld.FixedTick(playingSession, kFixedDeltaSeconds);
	}

	DebugStatsServices stats{};
	std::vector<ExtractedObject> extracted{};
	sessionWorld.Extract(extracted, 1.0f, playingSession, stats);
	const Transform* fallen = sessionWorld.GetTransform(sessionBody);
	if (fallen == nullptr || !(fallen->position.y < 3.5f) || !IsNearlyEqual(fallen->scale.x, 2.0f)
		|| !IsNearlyEqual(fallen->scale.y, 3.0f) || !IsNearlyEqual(fallen->scale.z, 4.0f)
		|| stats.extractedObjectCount != 1 || extracted.size() != 1
		|| !IsNearlyEqual(ExtractedTranslationY(extracted[0]), fallen->position.y))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"FixedTick should simulate the owned physics without changing scale");
	}

	const float pausedHeight = fallen->position.y;
	playingSession.mode = SessionMode::Paused;
	sessionWorld.FixedTick(playingSession, kFixedDeltaSeconds);
	extracted.clear();
	sessionWorld.Extract(extracted, 0.0f, playingSession, stats);
	EditorCameraServices pausedEditorCamera{};
	pausedEditorCamera.valid = true;
	pausedEditorCamera.view.viewportWidth = 7;
	ExtractedView pausedView{};
	sessionWorld.ExtractView(pausedView, 640, 480, &pausedEditorCamera);
	SceneContentServices heldContent{};
	heldContent.loaded = true;
	SessionServices heldSession{};
	heldSession.mode = SessionMode::Paused;
	WorldServices heldServices{};
	const bool heldRelative = sessionWorld.Tick(
		0.016f, heldSession, heldServices, heldContent, nullptr, false, false, nullptr, nullptr);
	if (!sessionWorld.IsPlaying() || heldRelative || heldServices.generation != 1
		|| !IsNearlyEqual(sessionWorld.GetTransform(sessionBody)->position.y, pausedHeight)
		|| extracted.size() != 1 || !IsNearlyEqual(ExtractedTranslationY(extracted[0]), pausedHeight)
		|| pausedView.viewportWidth != 640)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Pause should keep the play scene, skip the step, and extract the play camera");
	}

	World playingClone = sessionWorld.Clone();
	World playingCopy{};
	playingCopy = sessionWorld;
	playingCopy.GetTransform(sessionBody)->position.y = 0.0f;
	if (playingClone.IsPlaying() || playingCopy.IsPlaying()
		|| !IsNearlyEqual(playingClone.GetTransform(sessionBody)->position.y, pausedHeight)
		|| !IsNearlyEqual(sessionWorld.GetTransform(sessionBody)->position.y, pausedHeight))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Copy and Clone should copy the simulated document and leave the play session behind");
	}

	RecordingLoader loader{};
	SceneContentServices content{};
	content.loader = &loader;
	content.loaded = true;
	WorldServices published{};
	SubsystemContext loadContext{};
	FrameContext loadFrame{};
	if (auto loaded = sessionWorld.LoadContent(content, published, loadContext, loadFrame, nullptr); !loaded
		|| HasNamedEntity(sessionWorld, "Loaded") || published.world != &sessionWorld || published.generation != 1)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Loading during play should update the editor snapshot");
	}

	EditorCameraServices editorCamera{};
	editorCamera.valid = true;
	editorCamera.view.viewportWidth = 7;
	ExtractedView sessionView{};
	sessionWorld.ExtractView(sessionView, 640, 480, &editorCamera);
	ExtractedLighting sessionLighting{};
	sessionWorld.GetLighting().ambientIntensity = 0.35f;
	sessionWorld.ExtractLighting(sessionLighting);
	if (sessionView.viewportWidth != 640 || !IsNearlyEqual(sessionLighting.ambientIntensity, 0.35f))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Play extraction should use the camera entity and the simulated lighting");
	}

	sessionWorld.EndPlay();
	const Transform* restored = sessionWorld.GetTransform(sessionBody);
	ExtractedView editorView{};
	sessionWorld.ExtractView(editorView, 640, 480, &editorCamera);
	extracted.clear();
	SessionServices editorSession{};
	sessionWorld.Extract(extracted, 1.0f, editorSession, stats);
	if (sessionWorld.IsPlaying() || sessionWorld.FindCamera().IsValid() || restored == nullptr
		|| !IsNearlyEqual(restored->position.y, 4.0f) || !IsNearlyEqual(restored->scale.y, 3.0f)
		|| !HasNamedEntity(sessionWorld, "Loaded") || editorView.viewportWidth != 7
		|| extracted.size() != 1 || !IsNearlyEqual(ExtractedTranslationY(extracted[0]), 4.0f)
		|| playingClone.IsPlaying() || !IsNearlyEqual(playingClone.GetTransform(sessionBody)->position.y, pausedHeight))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Ending play should restore the editor document, including content loaded during play");
	}

	SessionServices session{};
	session.mode = SessionMode::Paused;
	WorldServices tickServices{};
	const bool pausedRelative = sessionWorld.Tick(
		0.016f, session, tickServices, content, nullptr, false, false, nullptr, nullptr);
	if (pausedRelative || session.mode != SessionMode::Editor || sessionWorld.IsPlaying()
		|| tickServices.world != &sessionWorld || tickServices.generation != 1)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"A paused request should not open a session");
	}

	session.mode = SessionMode::Playing;
	const bool playingRelative = sessionWorld.Tick(
		0.016f, session, tickServices, content, nullptr, false, false, &playCameraPosition, &playCameraForward);
	const uint32_t playingGeneration = tickServices.generation;
	session.mode = SessionMode::Editor;
	const bool editorRelative = sessionWorld.Tick(
		0.016f, session, tickServices, content, nullptr, false, false, nullptr, nullptr);
	if (!playingRelative && !editorRelative && !sessionWorld.IsPlaying() && playingGeneration == 2
		&& tickServices.generation == 3
		&& IsNearlyEqual(sessionWorld.GetTransform(sessionBody)->position.y, 4.0f))
	{
		LOG_INFO(LogCategory::ECS, "WorldTests passed");
		return MakeOk();
	}

	sessionWorld.EndPlay();
	return FailRuntime(
		LogCategory::ECS,
		ErrorCode::InvalidArgument,
		"Tick should advance generation when a session starts and stops");
}
