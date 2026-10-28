#pragma once

#include "Engine/Physics/PhysicsTypes.h"
#include "Engine/World/Component.h"

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <vector>

// Points and indices are owned here. PhysicsBodyDesc spans are only valid during Bind.
struct RigidBodyShape
{
	PhysicsShapeType type = PhysicsShapeType::Sphere;
	DirectX::XMFLOAT3 halfExtent{ 0.5f, 0.5f, 0.5f };
	float radius = 0.5f;
	float halfHeight = 0.5f;
	std::string mesh{};
	std::vector<DirectX::XMFLOAT3> points{};
	std::vector<uint32_t> indices{};
};

// Authored rigid body. The simulated PhysicsBodyId lives in EntityPhysics.
struct RigidBody : Component
{
	PhysicsMotion motion = PhysicsMotion::Dynamic;
	float friction = 0.2f;
	float restitution = 0.0f;
	float mass = 0.0f;
	RigidBodyShape shape{};
};
