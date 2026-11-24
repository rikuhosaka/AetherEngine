#include "Runtime/Engine/EngineInstance/Test/WorldExtractTest.h"

#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Core/Math/Matrix.h"
#include "Runtime/Core/Math/Transform.h"
#include "Runtime/Engine/EngineInstance/BuildRenderFrame.h"
#include "Runtime/Engine/World/Component/LightComponent.h"
#include "Runtime/Engine/World/Component/Renderable.h"
#include "Runtime/Engine/World/DebugStatsServices.h"
#include "Runtime/Engine/World/Lighting.h"
#include "Runtime/Engine/World/World.h"
#include "Runtime/Engine/World/WorldCameraSample.h"

#include <DirectXMath.h>

#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace
{
[[nodiscard]] bool IsNearlyEqual(float left, float right, float epsilon = 0.0001f)
{
	return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] RenderFrame ExtractFrame(
	const World& world,
	std::span<const RenderPose> poses = {})
{
	RenderFrame frame{};
	DebugStatsServices stats{};
	BuildRenderFrame(world, WorldCameraSample{}, 0, 0, poses, stats, frame);
	return frame;
}

[[nodiscard]] bool SameId(const RenderObjectId& objectId, EntityId entity)
{
	return objectId.Index == entity.Index && objectId.Generation == entity.Generation;
}

[[nodiscard]] Result<void> TestLightExtract()
{
	World empty{};
	const LightingSettings& settings = empty.GetLighting();
	const RenderFrame fallback = ExtractFrame(empty);
	if (!IsNearlyEqual(fallback.lighting.mainLightDirection[0], settings.mainLightDirection[0])
		|| !IsNearlyEqual(fallback.lighting.mainLightDirection[1], settings.mainLightDirection[1])
		|| !IsNearlyEqual(fallback.lighting.mainLightIntensity, settings.mainLightIntensity)
		|| !IsNearlyEqual(fallback.lighting.ambientIntensity, settings.ambientIntensity))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Lighting without a light component should keep the world lighting");
	}

	World world{};
	const EntityId entity = world.Spawn(WorldSpawnDesc{});
	LightComponent light{};
	light.color[0] = 0.2f;
	light.color[1] = 0.4f;
	light.color[2] = 0.6f;
	light.intensity = 3.5f;
	if (!world.Add<LightComponent>(entity, light))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Light extract test should add a light");
	}

	const RenderFrame extracted = ExtractFrame(world);
	if (!IsNearlyEqual(extracted.lighting.mainLightColor[0], 0.2f)
		|| !IsNearlyEqual(extracted.lighting.mainLightColor[1], 0.4f)
		|| !IsNearlyEqual(extracted.lighting.mainLightColor[2], 0.6f)
		|| !IsNearlyEqual(extracted.lighting.mainLightIntensity, 3.5f)
		|| !IsNearlyEqual(extracted.lighting.mainLightDirection[2], 1.0f)
		|| !IsNearlyEqual(extracted.lighting.ambientIntensity, world.GetLighting().ambientIntensity))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"The first light component should replace the main light and keep ambient");
	}

	return MakeOk();
}
} // namespace

Result<void> RunWorldExtractTests()
{
	if (auto light = TestLightExtract(); !light)
	{
		return light;
	}

	World world{};
	WorldSpawnDesc spawnDesc{};
	spawnDesc.transform.position = { 5.0f, 2.0f, -1.0f };
	const EntityId first = world.Spawn(std::move(spawnDesc));
	Renderable firstRenderable{};
	firstRenderable.visible = true;
	firstRenderable.mesh = WorldMeshId{ 3, 1 };
	firstRenderable.material = WorldMaterialId{ 4, 2 };
	firstRenderable.layerMask = WorldLayer::Opaque | WorldLayer::Shadow;
	WorldMaterialParameterBlock parameter{};
	parameter.bindingSlot = 2;
	parameter.data = { std::byte{ 1 }, std::byte{ 2 }, std::byte{ 3 } };
	firstRenderable.overrides.parameters.push_back(parameter);
	if (!world.Add<Renderable>(first, std::move(firstRenderable)))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extract test should add a renderable");
	}

	RenderFrame frame = ExtractFrame(world);
	if (frame.objects.size() != 1)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Spawned entity should extract as one object");
	}

	const ExtractedObjectSnapshot& firstObject = frame.objects[0];
	if (!SameId(firstObject.objectId, first))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extracted object id should match entity id");
	}

	if (firstObject.mesh.Index != 3 || firstObject.mesh.Generation != 1
		|| firstObject.material.Index != 4 || firstObject.material.Generation != 2
		|| firstObject.layerMask != (WorldLayer::Opaque | WorldLayer::Shadow))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extracted handles should match world ids");
	}

	if (firstObject.overrides.parameters.size() != 1
		|| firstObject.overrides.parameters[0].bindingSlot != 2
		|| firstObject.overrides.parameters[0].data.size() != 3
		|| firstObject.overrides.parameters[0].data[0] != std::byte{ 1 }
		|| firstObject.overrides.parameters[0].data[2] != std::byte{ 3 })
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extracted material parameters should copy the component bytes");
	}

	world.Get<Renderable>(first)->overrides.parameters[0].data[0] = std::byte{ 9 };
	if (firstObject.overrides.parameters[0].data[0] != std::byte{ 1 })
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extracted material parameters should be independent of the component");
	}

	const DirectX::XMMATRIX worldMatrix = Aether::Math::LoadMatrixFromHlsl(firstObject.worldMatrix);
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
	frame = ExtractFrame(world);
	if (!frame.objects.empty())
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Destroyed entity should not extract");
	}

	WorldSpawnDesc reusedDesc{};
	const EntityId second = world.Spawn(std::move(reusedDesc));
	Renderable reusedRenderable{};
	reusedRenderable.visible = true;
	if (!world.Add<Renderable>(second, std::move(reusedRenderable)))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extract test should add a renderable");
	}
	frame = ExtractFrame(world);
	if (frame.objects.size() != 1 || !SameId(frame.objects[0].objectId, second))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extract should use the reused entity generation");
	}

	const EntityId unrendered = world.Spawn(WorldSpawnDesc{});
	frame = ExtractFrame(world);
	bool sawUnrendered = false;
	for (const ExtractedObjectSnapshot& object : frame.objects)
	{
		if (SameId(object.objectId, unrendered))
		{
			sawUnrendered = true;
		}
	}
	if (frame.objects.size() != 1 || sawUnrendered)
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"An entity without a renderable should not extract");
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
	const EntityId child = world.Spawn(std::move(childDesc));
	Renderable childRenderable{};
	childRenderable.visible = true;
	if (!world.Add<Renderable>(child, std::move(childRenderable)))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extract test should add a child renderable");
	}

	frame = ExtractFrame(world);
	const ExtractedObjectSnapshot* extractedChild = nullptr;
	for (const ExtractedObjectSnapshot& object : frame.objects)
	{
		if (SameId(object.objectId, child))
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
	frame = ExtractFrame(world);
	for (const ExtractedObjectSnapshot& object : frame.objects)
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
	Renderable staleRenderable{};
	staleRenderable.visible = true;
	if (!world.Add<Renderable>(staleChild, std::move(staleRenderable)))
	{
		return FailRuntime(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Extract test should add a stale child renderable");
	}
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

	frame = ExtractFrame(world);
	const ExtractedObjectSnapshot* extractedStale = nullptr;
	for (const ExtractedObjectSnapshot& object : frame.objects)
	{
		if (SameId(object.objectId, staleChild))
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
