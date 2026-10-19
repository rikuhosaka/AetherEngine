#pragma once

struct ExtractedLighting;

struct SceneLightingSettings
{
	float ambientColor[3] { 0.15f, 0.15f, 0.18f };
	float ambientIntensity = 1.0f;
	float mainLightColor[3] { 1.0f, 0.97f, 0.92f };
	float mainLightIntensity = 1.2f;
	float mainLightDirection[3] { 0.3f, -1.0f, 0.2f };
};

void BuildExtractedLighting(const SceneLightingSettings& settings, ExtractedLighting& outLighting);
