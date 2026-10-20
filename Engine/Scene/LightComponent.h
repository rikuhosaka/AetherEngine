#pragma once

// Directional light. Direction is the entity world matrix forward. Ambient stays on the scene.
struct LightComponent
{
	float color[3]{ 1.0f, 0.97f, 0.92f };
	float intensity = 1.2f;
};
