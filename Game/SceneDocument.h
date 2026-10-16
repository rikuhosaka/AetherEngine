#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Physics/EntityPhysics.h"
#include "Engine/World/WorldDocument.h"
#include "Game/WorldRenderBridge.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Points and indices are owned here. PhysicsBodyDesc spans are valid only during Bind.
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
	MeshHandle mesh{};
};

struct SceneRenderResources
{
	MaterialHandle material{};
	TextureHandle baseColor{};
	std::optional<uint32_t> materialConstantsSlot{};
};

[[nodiscard]] Result<std::string> WriteSceneDocument(const SceneDocument& document);
[[nodiscard]] Result<SceneDocument> ReadSceneDocument(std::string_view text);

// Resolves mesh keys, copies tint, and binds parentless bodies.
// A failure unbinds bodies from this call and destroys entities from this call.
[[nodiscard]] Result<std::vector<WorldDocumentBinding>> InstantiateScene(
	World& world,
	EntityPhysics& physics,
	const SceneDocument& document,
	std::span<const SceneMeshRef> meshes,
	const SceneRenderResources& render);

// Copies live names, local transforms, and hierarchy. Mesh keys, tint, and bodies stay authored.
[[nodiscard]] Result<SceneDocument> CaptureScene(
	const World& world,
	const SceneDocument& authored,
	std::span<const WorldDocumentBinding> bindings);
