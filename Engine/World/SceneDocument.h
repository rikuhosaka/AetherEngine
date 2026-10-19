#pragma once

#include "Engine/World/Renderable.h"
#include "Engine/World/WorldDocument.h"
#include "Engine/Physics/PhysicsWorld.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Points and indices are owned here. PhysicsBodyDesc spans are only valid during Bind.
struct SceneShape
{
	PhysicsShapeType type = PhysicsShapeType::Sphere;
	DirectX::XMFLOAT3 halfExtent{ 0.5f, 0.5f, 0.5f };
	float radius = 0.5f;
	float halfHeight = 0.5f;
	std::vector<DirectX::XMFLOAT3> points{};
	std::vector<uint32_t> indices{};
};

struct SceneBody
{
	PhysicsMotion motion = PhysicsMotion::Dynamic;
	float friction = 0.2f;
	float restitution = 0.0f;
	float mass = 0.0f;
	SceneShape shape{};
};

// Parallel to SceneDocument::world. A missing body means the entity has no rigid body.
struct SceneEntity
{
	uint32_t id = 0;
	std::string mesh{};
	float tint[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
	std::optional<SceneBody> body{};
};

struct SceneDocument
{
	WorldDocument world{};
	std::vector<SceneEntity> entities{};
};

struct SceneMeshRef
{
	std::string key{};
	WorldMeshId mesh{};
};

struct SceneRenderResources
{
	WorldMaterialId material{};
	WorldTextureId baseColor{};
	std::optional<uint32_t> materialConstantsSlot{};
};
