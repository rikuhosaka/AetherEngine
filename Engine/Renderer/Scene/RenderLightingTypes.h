#pragma once

struct ExtractedLighting
{
	float ambientColor[3]{ 0.15f, 0.15f, 0.18f };
	float ambientIntensity = 1.0f;
	float mainLightColor[3]{ 1.0f, 0.97f, 0.92f };
	float mainLightIntensity = 1.2f;
	float mainLightDirection[3]{ 0.3f, -1.0f, 0.2f };
	float pad0 = 0.0f;
};
