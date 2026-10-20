#include "Engine/Scene/SceneLighting.h"

#include "Engine/Renderer/Scene/RenderLightingTypes.h"
#include "Engine/Scene/LightComponent.h"
#include "Engine/World/World.h"

#include <DirectXMath.h>

void BuildExtractedLighting(
	const SceneLightingSettings& settings,
	const World& world,
	ExtractedLighting& outLighting)
{
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
