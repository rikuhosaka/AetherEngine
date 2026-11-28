#include "Runtime/Engine/EngineInstance/ModelSceneSpawn.h"

#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Core/Math/Matrix.h"
#include "Runtime/Engine/EngineInstance/WorldRenderBridge.h"
#include "Runtime/Engine/World/Component/Renderable.h"
#include "Runtime/Engine/World/Component/TransformComponent.h"
#include "Runtime/Engine/World/World.h"
#include "Runtime/Renderer/Scene/FbxSceneAssets.h"

#include <DirectXMath.h>

#include <cmath>
#include <cstring>
#include <vector>

namespace
{
[[nodiscard]] const WorldDocumentBinding* FindBinding(
	std::span<const WorldDocumentBinding> bindings,
	uint32_t id)
{
	for (const WorldDocumentBinding& binding : bindings)
	{
		if (binding.id == id)
		{
			return &binding;
		}
	}

	return nullptr;
}

DirectX::XMMATRIX LoadRowMajorMatrix(const float matrix4x4[16])
{
	DirectX::XMFLOAT4X4 loaded{};
	std::memcpy(loaded.m, matrix4x4, sizeof(loaded.m));
	return DirectX::XMLoadFloat4x4(&loaded);
}

[[nodiscard]] Result<TransformComponent> TransformFromDxMatrix(DirectX::FXMMATRIX matrix)
{
	DirectX::XMVECTOR scale{};
	DirectX::XMVECTOR rotation{};
	DirectX::XMVECTOR translation{};
	if (!DirectX::XMMatrixDecompose(&scale, &rotation, &translation, matrix))
	{
		return FailInternal<TransformComponent>(LogCategory::Asset, ErrorCode::InvalidArgument,
			"FBX node transform could not be decomposed into translation, rotation, and scale");
	}

	TransformComponent transform{};
	DirectX::XMStoreFloat3(&transform.position, translation);
	DirectX::XMStoreFloat4(&transform.rotation, rotation);
	DirectX::XMStoreFloat3(&transform.scale, scale);
	return MakeOk(std::move(transform));
}

[[nodiscard]] bool MatricesNearlyEqual(DirectX::FXMMATRIX left, DirectX::FXMMATRIX right)
{
	DirectX::XMFLOAT4X4 leftStored{};
	DirectX::XMFLOAT4X4 rightStored{};
	DirectX::XMStoreFloat4x4(&leftStored, left);
	DirectX::XMStoreFloat4x4(&rightStored, right);
	constexpr float kEpsilon = 0.001f;
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			if (std::fabs(leftStored.m[row][column] - rightStored.m[row][column]) > kEpsilon)
			{
				return false;
			}
		}
	}

	return true;
}

[[nodiscard]] Result<void> SpawnModel(World& world, EntityId parent, const FbxSceneAssets& model)
{
	if (!model.IsReady() || !world.IsAlive(parent))
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"SpawnUploadedModels requires initialized GPU assets and a live parent");
	}

	std::vector<EntityId> spawnedEntities;
	auto rollback = [&world, &spawnedEntities]() {
		for (const EntityId entity : spawnedEntities)
		{
			world.Destroy(entity);
		}
	};

	std::vector<EntityId> nodeEntities;
	nodeEntities.reserve(model.Nodes().size());
	for (const FbxNodeRecord& node : model.Nodes())
	{
		const Result<TransformComponent> local = TransformFromDxMatrix(LoadRowMajorMatrix(node.localTransform));
		if (!local)
		{
			rollback();
			return MakeFail(local.error.code, local.error.message);
		}

		WorldSpawnDesc desc{};
		desc.name = node.name.empty() ? "ModelNode" : node.name;
		desc.transform = local.value;
		const EntityId nodeEntity = world.Spawn(std::move(desc));
		nodeEntities.push_back(nodeEntity);
		spawnedEntities.push_back(nodeEntity);
	}

	for (size_t nodeIndex = 0; nodeIndex < model.Nodes().size(); ++nodeIndex)
	{
		const int32_t parentIndex = model.Nodes()[nodeIndex].parentIndex;
		if (parentIndex >= 0 && static_cast<size_t>(parentIndex) >= nodeEntities.size())
		{
			rollback();
			return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
				"Model node parent could not be attached");
		}

		const EntityId nodeParent = parentIndex < 0
			? parent
			: nodeEntities[static_cast<size_t>(parentIndex)];
		if (!world.SetParent(nodeEntities[nodeIndex], nodeParent))
		{
			rollback();
			return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
				"Model node parent could not be attached");
		}
	}

	const DirectX::XMMATRIX placementMatrix = world.GetWorldMatrix(parent);
	for (const FbxSubmeshRecord& submesh : model.Submeshes())
	{
		if (submesh.nodeIndex >= nodeEntities.size())
		{
			rollback();
			return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
				"Model submesh node index is out of range");
		}

		WorldSpawnDesc desc{};
		desc.name = submesh.name;
		desc.parent = nodeEntities[submesh.nodeIndex];
		const EntityId submeshEntity = world.Spawn(std::move(desc));
		spawnedEntities.push_back(submeshEntity);

		Renderable renderable{};
		renderable.mesh = ToWorldMeshId(submesh.mesh);
		renderable.material = ToWorldMaterialId(model.GetMaterial());
		renderable.submeshIndex = submesh.submeshIndex;
		renderable.layerMask = WorldLayer::Opaque | WorldLayer::Shadow;
		renderable.visible = true;
		renderable.overrides.baseColor = ToWorldTextureId(submesh.baseColor);
		renderable.overrides.normal = ToWorldTextureId(submesh.normal);
		renderable.overrides.specular = ToWorldTextureId(submesh.specular);
		if (model.MaterialConstantsSlot().has_value())
		{
			WorldMaterialParameterBlock materialBlock{};
			materialBlock.bindingSlot = *model.MaterialConstantsSlot();
			const LitMaterialConstants& constants = model.MaterialConstants();
			const auto* bytes = reinterpret_cast<const std::byte*>(&constants);
			materialBlock.data.assign(bytes, bytes + sizeof(constants));
			renderable.overrides.parameters.push_back(std::move(materialBlock));
		}

		if (!world.Add<Renderable>(submeshEntity, std::move(renderable)))
		{
			rollback();
			return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
				"Model submesh renderable was rejected");
		}

		const DirectX::XMMATRIX resolved = world.GetWorldMatrix(submeshEntity);
		const DirectX::XMMATRIX baked = DirectX::XMMatrixMultiply(
			Aether::Math::LoadMatrixFromHlsl(submesh.worldMatrix),
			placementMatrix);
		if (!MatricesNearlyEqual(resolved, baked))
		{
			rollback();
			return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
				"Model hierarchy world matrix does not match the baked node transform");
		}
	}

	LOG_INFO_F(LogCategory::Core, "SpawnUploadedModels spawned {} submeshes", model.Submeshes().size());
	return MakeOk();
}
} // namespace

Result<void> SpawnUploadedModels(
	World& world,
	std::span<const WorldDocumentBinding> bindings,
	std::span<const SceneModelAsset> models)
{
	for (const SceneModelAsset& model : models)
	{
		const WorldDocumentBinding* binding = FindBinding(bindings, model.documentId);
		if (binding == nullptr || model.assets == nullptr)
		{
			return FailInternal(LogCategory::Asset, ErrorCode::InvalidArgument,
				"Uploaded model has no scene entity");
		}

		if (auto spawned = SpawnModel(world, binding->entity, *model.assets); !spawned)
		{
			return spawned;
		}
	}

	return MakeOk();
}
