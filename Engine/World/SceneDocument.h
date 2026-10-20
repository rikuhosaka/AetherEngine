#pragma once

#include "Engine/Camera/CameraComponent.h"
#include "Engine/Scene/LightComponent.h"
#include "Engine/World/Renderable.h"
#include "Engine/World/RigidBody.h"
#include "Engine/World/WorldDocument.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Parallel to SceneDocument::world. A missing component means the entity does not have it.
struct SceneEntity
{
	uint32_t id = 0;
	std::string mesh{};
	float tint[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
	std::optional<RigidBody> body{};
	std::optional<CameraComponent> camera{};
	std::optional<LightComponent> light{};
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
