#pragma once

#include <cstdint>

enum class PhysicsMotion : uint8_t
{
	Static,
	Dynamic,
	Kinematic,
};

enum class PhysicsShapeType : uint8_t
{
	Box,
	Sphere,
	Capsule,
	ConvexHull,
	Mesh,
};
