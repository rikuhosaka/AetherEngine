#include "Game/Lighting/DefaultLightingSetup.h"

#include "Engine/Renderer/Scene/RenderLightingTypes.h"

void BuildExtractedLighting(const SceneLightingSettings& settings, ExtractedLighting& outLighting)
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
}
