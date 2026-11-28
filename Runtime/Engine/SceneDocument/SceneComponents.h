#pragma once

#include "Runtime/Physics/PhysicsTypes.h"

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <vector>

struct SceneTransform
{
	DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
};

struct SceneRigidBodyShape
{
	PhysicsShapeType type = PhysicsShapeType::Sphere;
	DirectX::XMFLOAT3 halfExtent{ 0.5f, 0.5f, 0.5f };
	float radius = 0.5f;
	float halfHeight = 0.5f;
	std::string mesh{};
	std::vector<DirectX::XMFLOAT3> points{};
	std::vector<uint32_t> indices{};
};

struct SceneRigidBody
{
	PhysicsMotion motion = PhysicsMotion::Dynamic;
	float friction = 0.2f;
	float restitution = 0.0f;
	float mass = 0.0f;
	SceneRigidBodyShape shape{};
};

struct SceneCamera
{
	float fovYDegrees = 60.0f;
	float nearPlane = 0.1f;
	float farPlane = 2000.0f;
};

struct SceneLight
{
	float color[3]{ 1.0f, 0.97f, 0.92f };
	float intensity = 1.2f;
};
