#include "Game/WorldExtract.h"

#include "Engine/Math/Matrix.h"
#include "Engine/Math/Transform.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/World/World.h"
#include "Game/WorldRenderBridge.h"

#include <DirectXMath.h>

#include <span>

static_assert(WorldLayer::Opaque == RenderLayer::Opaque);
static_assert(WorldLayer::Shadow == RenderLayer::Shadow);
static_assert(WorldLayer::Transparent == RenderLayer::Transparent);

void ExtractWorld(const World& world, std::vector<ExtractedObject>& outObjects)
{
	world.ForEachAlive([&outObjects](EntityId id, const Transform& transform, const Renderable& renderable) {
		ExtractedObject object{};
		object.objectId.Index = id.Index;
		object.objectId.Generation = id.Generation;
		object.mesh = ToMeshHandle(renderable.mesh);
		object.material = ToMaterialHandle(renderable.material);
		object.submeshIndex = renderable.submeshIndex;
		object.layerMask = renderable.layerMask;
		object.visible = renderable.visible;

		const DirectX::XMVECTOR translation = DirectX::XMLoadFloat3(&transform.position);
		const DirectX::XMVECTOR rotation = DirectX::XMLoadFloat4(&transform.rotation);
		const DirectX::XMVECTOR scale = DirectX::XMLoadFloat3(&transform.scale);
		const DirectX::XMMATRIX worldMatrix = Aether::Math::TRS(translation, rotation, scale);
		Aether::Math::StoreMatrixForHlsl(object.worldMatrix, worldMatrix);

		object.overrides.baseColor = ToTextureHandle(renderable.overrides.baseColor);
		object.overrides.normal = ToTextureHandle(renderable.overrides.normal);
		object.overrides.parameters.reserve(renderable.overrides.parameters.size());
		for (const WorldMaterialParameterBlock& block : renderable.overrides.parameters)
		{
			MaterialParameterBlock extractedBlock{};
			extractedBlock.bindingSlot = block.bindingSlot;
			extractedBlock.data = std::span<const std::byte>(block.data);
			object.overrides.parameters.push_back(extractedBlock);
		}

		outObjects.push_back(std::move(object));
	});
}
