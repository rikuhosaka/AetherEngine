#pragma once

#include "Runtime/Engine/SceneDocument/SceneComponents.h"
#include "Runtime/Engine/World/Component/CameraComponent.h"
#include "Runtime/Engine/World/Component/LightComponent.h"
#include "Runtime/Engine/World/Component/RigidBody.h"
#include "Runtime/Engine/World/Component/TransformComponent.h"

[[nodiscard]] TransformComponent ToTransformComponent(const SceneTransform& transform);
[[nodiscard]] SceneTransform ToSceneTransform(const TransformComponent& transform);

[[nodiscard]] RigidBody ToRigidBody(const SceneRigidBody& body);
[[nodiscard]] SceneRigidBody ToSceneRigidBody(const RigidBody& body);

[[nodiscard]] CameraComponent ToCameraComponent(const SceneCamera& camera);
[[nodiscard]] SceneCamera ToSceneCamera(const CameraComponent& camera);

[[nodiscard]] LightComponent ToLightComponent(const SceneLight& light);
[[nodiscard]] SceneLight ToSceneLight(const LightComponent& light);
