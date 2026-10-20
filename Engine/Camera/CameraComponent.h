#pragma once

// Lens for an entity camera. Position and orientation come from the entity transform.
struct CameraComponent
{
	float fovYDegrees = 60.0f;
	float nearPlane = 0.1f;
	float farPlane = 2000.0f;
};
