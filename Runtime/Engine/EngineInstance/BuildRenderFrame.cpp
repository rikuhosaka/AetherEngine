#include "Runtime/Engine/EngineInstance/BuildRenderFrame.h"

#include "Runtime/Engine/EngineInstance/CameraViewBuild.h"
#include "Runtime/Engine/EngineInstance/WorldRenderBridge.h"
#include "Runtime/Core/Math/Matrix.h"
#include "Runtime/Engine/World/Component/LightComponent.h"
#include "Runtime/Engine/World/Component/Renderable.h"
#include "Runtime/Engine/World/DebugStatsServices.h"
#include "Runtime/Engine/World/Lighting.h"
#include "Runtime/Engine/World/World.h"

#include <DirectXMath.h>

namespace
{
void BuildLighting(const World& world, ExtractedLighting& outLighting)
{
	const LightingSettings& settings = world.GetLighting();
	outLighting = ExtractedLighting{};
	outLighting.ambientColor[0] = settings.ambientColor[0];
	outLighting.ambientColor[1] = settings.ambientColor[1];
	outLighting.ambientColor[2] = settings.ambientColor[2];
	outLighting.ambientIntensity = settings.ambientIntensity;
	outLighting.mainLightColor[0] = settings.mainLightColor[0];
	outLighting.mainLightColor[1] = settings.mainLightColor[1];
	outLighting.mainLightColor[2] = settings.mainLightColor[2];
	outLighting.mainLightIntensity = settings.mainLightIntensity;
	outLighting.mainLightDirection[0] = settings.mainLightDirection[0];
	outLighting.mainLightDirection[1] = settings.mainLightDirection[1];
	outLighting.mainLightDirection[2] = settings.mainLightDirection[2];

	const LightComponent* light = nullptr;
	EntityId lightEntity{};
	world.ForEach<LightComponent>([&light, &lightEntity](EntityId id, const LightComponent& component) {
		if (light == nullptr)
		{
			light = &component;
			lightEntity = id;
		}
	});
	if (light == nullptr)
	{
		return;
	}

	outLighting.mainLightColor[0] = light->color[0];
	outLighting.mainLightColor[1] = light->color[1];
	outLighting.mainLightColor[2] = light->color[2];
	outLighting.mainLightIntensity = light->intensity;

	const DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(
		DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
		world.GetWorldMatrix(lightEntity)));
	DirectX::XMFLOAT3 direction{};
	DirectX::XMStoreFloat3(&direction, forward);
	outLighting.mainLightDirection[0] = direction.x;
	outLighting.mainLightDirection[1] = direction.y;
	outLighting.mainLightDirection[2] = direction.z;
}

ExtractedObjectSnapshot MakeExtractedObject(
	const World& world,
	EntityId id,
	const Renderable& renderable,
	std::span<const RenderPose> poses)
{
	ExtractedObjectSnapshot object{};
	object.objectId = RenderObjectId{ id.Index, id.Generation };
	object.mesh = ToMeshHandle(renderable.mesh);
	object.material = ToMaterialHandle(renderable.material);
	object.submeshIndex = renderable.submeshIndex;
	object.layerMask = renderable.layerMask;
	object.visible = renderable.visible;
	Aether::Math::StoreMatrixForHlsl(object.worldMatrix, world.GetWorldMatrix(id, poses));

	object.overrides.baseColor = ToTextureHandle(renderable.overrides.baseColor);
	object.overrides.normal = ToTextureHandle(renderable.overrides.normal);
	object.overrides.parameters.reserve(renderable.overrides.parameters.size());
	for (const WorldMaterialParameterBlock& block : renderable.overrides.parameters)
	{
		StoredMaterialParameterBlock stored{};
		stored.bindingSlot = block.bindingSlot;
		stored.data = block.data;
		object.overrides.parameters.push_back(std::move(stored));
	}

	return object;
}
} // namespace

void BuildRenderFrame(
	const World& world,
	const WorldCameraSample& camera,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	std::span<const RenderPose> poses,
	DebugStatsServices& stats,
	RenderFrame& out)
{
	out = {};
	if (camera.found)
	{
		BuildExtractedView(camera.state, viewportWidth, viewportHeight, out.view);
	}

	BuildLighting(world, out.lighting);

	world.ForEach<Renderable>([&world, &out, poses](EntityId id, const Renderable& renderable) {
		out.objects.push_back(MakeExtractedObject(world, id, renderable, poses));
	});
	stats.extractedObjectCount = static_cast<uint32_t>(out.objects.size());
}
