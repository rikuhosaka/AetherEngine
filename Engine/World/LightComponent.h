#pragma once

#include "Engine/World/Component.h"

// Directional light. Direction is the entity world matrix forward. Ambient stays on the world.
struct LightComponent : Component
{
	float color[3]{ 1.0f, 0.97f, 0.92f };
	float intensity = 1.2f;
};
