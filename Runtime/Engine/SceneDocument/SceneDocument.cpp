#include "Runtime/Engine/SceneDocument/SceneDocument.h"

#include "Runtime/Engine/SceneDocument/SceneComponentMap.h"
#include "Runtime/Engine/SceneDocument/Serializer/CameraComponentSerializer.h"
#include "Runtime/Core/Json/JsonFields.h"
#include "Runtime/Engine/SceneDocument/Serializer/LightComponentSerializer.h"
#include "Runtime/Engine/SceneDocument/Serializer/RigidBodySerializer.h"
#include "Runtime/Engine/SceneDocument/Serializer/TransformSerializer.h"
#include "Runtime/Engine/World/World.h"
#include "Runtime/Renderer/Scene/FbxSceneAssets.h"
#include "ThirdParty/nlohmann/json.hpp"

#include <cctype>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>

namespace
{
using Json = nlohmann::json;

[[nodiscard]] bool IsModelAssetPath(const std::filesystem::path& path)
{
	if (path.empty() || path.is_absolute())
	{
		return false;
	}

	for (const std::filesystem::path& part : path)
	{
		if (part == "..")
		{
			return false;
		}
	}

	std::string extension = path.extension().string();
	for (char& character : extension)
	{
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	return extension == ".fbx" || extension == ".gltf" || extension == ".glb";
}

[[nodiscard]] bool SameIds(const SceneDocument& document)
{
	if (document.world.size() != document.entities.size())
	{
		return false;
	}

	for (size_t index = 0; index < document.world.size(); ++index)
	{
		if (document.world[index].id != document.entities[index].id)
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] Result<SceneEntity> ReadSceneEntity(const Json& value, const WorldDocumentEntity& worldEntity)
{
	if (!value.is_object())
	{
		return FailRuntime<SceneEntity>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document entity must be an object");
	}

	const Json* renderable = JsonFields::FindField(value, "renderable");
	const Json* model = JsonFields::FindField(value, "model");
	SceneEntity entity{};
	entity.id = worldEntity.id;
	const Json* transformValue = JsonFields::FindField(value, TransformSerializer::kKey);
	if (transformValue == nullptr)
	{
		return FailRuntime<SceneEntity>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document transform is missing a field");
	}

	Result<SceneTransform> transform = TransformSerializer::Read(*transformValue);
	if (!transform)
	{
		return MakeFail<SceneEntity>(transform.error.code, transform.error.message);
	}

	entity.transform = std::move(transform.value);
	if (model != nullptr)
	{
		if (!model->is_string() || !IsModelAssetPath(model->get<std::string>()) || renderable != nullptr
			|| worldEntity.hasRenderable)
		{
			return FailRuntime<SceneEntity>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document model must be an FBX, glTF, or GLB path without a renderable");
		}

		entity.model = model->get<std::string>();
		if (JsonFields::FindField(value, RigidBodySerializer::kKey) != nullptr)
		{
			return FailRuntime<SceneEntity>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document model cannot have physics");
		}
	}
	else
	{
		if (renderable == nullptr || !renderable->is_object())
		{
			return FailRuntime<SceneEntity>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document renderable is missing");
		}

		const Json* mesh = JsonFields::FindField(*renderable, "mesh");
		const Json* tint = JsonFields::FindField(*renderable, "tint");
		if (mesh == nullptr || !mesh->is_string() || mesh->get<std::string>().empty() || tint == nullptr
			|| !JsonFields::ReadFloats(*tint, entity.tint, 4))
		{
			return FailRuntime<SceneEntity>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document mesh or tint is missing");
		}

		entity.mesh = mesh->get<std::string>();
	}
	if (const Json* physics = JsonFields::FindField(value, RigidBodySerializer::kKey))
	{
		if (worldEntity.parent != 0)
		{
			return FailRuntime<SceneEntity>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document physics requires a root entity");
		}

		Result<SceneRigidBody> body = RigidBodySerializer::Read(*physics);
		if (!body)
		{
			return MakeFail<SceneEntity>(body.error.code, body.error.message);
		}

		entity.body = std::move(body.value);
	}

	if (const Json* camera = JsonFields::FindField(value, CameraComponentSerializer::kKey))
	{
		Result<SceneCamera> component = CameraComponentSerializer::Read(*camera);
		if (!component)
		{
			return MakeFail<SceneEntity>(component.error.code, component.error.message);
		}

		entity.camera = std::move(component.value);
	}

	if (const Json* light = JsonFields::FindField(value, LightComponentSerializer::kKey))
	{
		Result<SceneLight> component = LightComponentSerializer::Read(*light);
		if (!component)
		{
			return MakeFail<SceneEntity>(component.error.code, component.error.message);
		}

		entity.light = std::move(component.value);
	}

	return MakeOk(std::move(entity));
}

[[nodiscard]] Result<SceneDocument> ReadDocument(std::string_view text)
{
	Result<WorldDocument> world = ReadWorldDocument(text);
	if (!world)
	{
		return MakeFail<SceneDocument>(world.error.code, world.error.message);
	}

	const Json root = Json::parse(text.begin(), text.end(), nullptr, false);
	const Json* entities = root.is_discarded() ? nullptr : JsonFields::FindField(root, "entities");
	if (entities == nullptr || !entities->is_array() || entities->size() != world.value.size())
	{
		return FailRuntime<SceneDocument>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document JSON is invalid");
	}

	SceneDocument document{};
	document.world = std::move(world.value);
	document.entities.reserve(document.world.size());
	for (size_t index = 0; index < document.world.size(); ++index)
	{
		Result<SceneEntity> entity = ReadSceneEntity((*entities)[index], document.world[index]);
		if (!entity)
		{
			return MakeFail<SceneDocument>(entity.error.code, entity.error.message);
		}

		document.entities.push_back(std::move(entity.value));
	}

	return MakeOk(std::move(document));
}

[[nodiscard]] const SceneMeshRef* FindMesh(std::span<const SceneMeshRef> meshes, std::string_view key)
{
	for (const SceneMeshRef& mesh : meshes)
	{
		if (mesh.key == key)
		{
			return &mesh;
		}
	}

	return nullptr;
}

[[nodiscard]] const WorldDocumentBinding* FindBinding(
	const std::vector<WorldDocumentBinding>& bindings,
	uint32_t id)
{
	for (const WorldDocumentBinding& binding : bindings)
	{
		if (binding.id == id)
		{
			return &binding;
		}
	}

	return nullptr;
}

[[nodiscard]] Json WriteEntity(const WorldDocumentEntity& worldEntity, const SceneEntity& entity)
{
	Json item = Json::object();
	item["id"] = worldEntity.id;
	item["name"] = worldEntity.name;
	item["parent"] = worldEntity.parent == 0 ? Json(nullptr) : Json(worldEntity.parent);
	item[TransformSerializer::kKey] = TransformSerializer::Write(entity.transform);
	if (!entity.model.empty())
	{
		item["model"] = entity.model;
	}
	else
	{
		item["renderable"] = Json::object({
			{ "mesh", entity.mesh },
			{ "submesh", worldEntity.submeshIndex },
			{ "layerMask", static_cast<uint32_t>(worldEntity.layerMask) },
			{ "visible", worldEntity.visible },
			{ "tint", Json::array({ entity.tint[0], entity.tint[1], entity.tint[2], entity.tint[3] }) },
		});
	}
	if (entity.body.has_value())
	{
		item[RigidBodySerializer::kKey] = RigidBodySerializer::Write(*entity.body);
	}

	if (entity.camera.has_value())
	{
		item[CameraComponentSerializer::kKey] = CameraComponentSerializer::Write(*entity.camera);
	}

	if (entity.light.has_value())
	{
		item[LightComponentSerializer::kKey] = LightComponentSerializer::Write(*entity.light);
	}

	return item;
}

void Rollback(World& world, const std::vector<WorldDocumentBinding>& spawned)
{
	for (const WorldDocumentBinding& binding : spawned)
	{
		world.Destroy(binding.entity);
	}
}
} // namespace

Result<SceneDocument> ReadSceneDocument(std::string_view text)
{
	try
	{
		return ReadDocument(text);
	}
	catch (const Json::exception&)
	{
		return FailRuntime<SceneDocument>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document JSON is invalid");
	}
}

Result<SceneDocument> LoadSceneDocument(const std::filesystem::path& path)
{
	std::ifstream file(path);
	if (!file)
	{
		return FailRuntime<SceneDocument>(
			LogCategory::ECS,
			ErrorCode::FileNotFound,
			"Scene document file could not be opened");
	}

	const std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	return ReadSceneDocument(text);
}

TransformComponent ToTransformComponent(const SceneTransform& transform)
{
	TransformComponent component{};
	component.position = transform.position;
	component.rotation = transform.rotation;
	component.scale = transform.scale;
	return component;
}

SceneTransform ToSceneTransform(const TransformComponent& transform)
{
	SceneTransform component{};
	component.position = transform.position;
	component.rotation = transform.rotation;
	component.scale = transform.scale;
	return component;
}

RigidBody ToRigidBody(const SceneRigidBody& body)
{
	RigidBody component{};
	component.motion = body.motion;
	component.friction = body.friction;
	component.restitution = body.restitution;
	component.mass = body.mass;
	component.shape.type = body.shape.type;
	component.shape.halfExtent = body.shape.halfExtent;
	component.shape.radius = body.shape.radius;
	component.shape.halfHeight = body.shape.halfHeight;
	component.shape.mesh = body.shape.mesh;
	component.shape.points = body.shape.points;
	component.shape.indices = body.shape.indices;
	return component;
}

SceneRigidBody ToSceneRigidBody(const RigidBody& body)
{
	SceneRigidBody component{};
	component.motion = body.motion;
	component.friction = body.friction;
	component.restitution = body.restitution;
	component.mass = body.mass;
	component.shape.type = body.shape.type;
	component.shape.halfExtent = body.shape.halfExtent;
	component.shape.radius = body.shape.radius;
	component.shape.halfHeight = body.shape.halfHeight;
	component.shape.mesh = body.shape.mesh;
	component.shape.points = body.shape.points;
	component.shape.indices = body.shape.indices;
	return component;
}

CameraComponent ToCameraComponent(const SceneCamera& camera)
{
	CameraComponent component{};
	component.fovYDegrees = camera.fovYDegrees;
	component.nearPlane = camera.nearPlane;
	component.farPlane = camera.farPlane;
	return component;
}

SceneCamera ToSceneCamera(const CameraComponent& camera)
{
	SceneCamera component{};
	component.fovYDegrees = camera.fovYDegrees;
	component.nearPlane = camera.nearPlane;
	component.farPlane = camera.farPlane;
	return component;
}

LightComponent ToLightComponent(const SceneLight& light)
{
	LightComponent component{};
	component.color[0] = light.color[0];
	component.color[1] = light.color[1];
	component.color[2] = light.color[2];
	component.intensity = light.intensity;
	return component;
}

SceneLight ToSceneLight(const LightComponent& light)
{
	SceneLight component{};
	component.color[0] = light.color[0];
	component.color[1] = light.color[1];
	component.color[2] = light.color[2];
	component.intensity = light.intensity;
	return component;
}

Result<std::vector<WorldDocumentBinding>> InstantiateScene(
	World& world,
	const SceneDocument& document,
	std::span<const SceneMeshRef> meshes,
	const SceneRenderResources& render)
{
	if (!SameIds(document))
	{
		return FailRuntime<std::vector<WorldDocumentBinding>>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document entities must match the world document");
	}

	Result<std::vector<WorldDocumentBinding>> spawned = Instantiate(world, document.world);
	if (!spawned)
	{
		return spawned;
	}

	for (size_t index = 0; index < document.entities.size(); ++index)
	{
		const SceneEntity& entity = document.entities[index];
		const WorldDocumentBinding* binding = FindBinding(spawned.value, entity.id);
		TransformComponent* liveTransform = binding == nullptr ? nullptr : world.GetTransform(binding->entity);
		if (binding == nullptr || liveTransform == nullptr)
		{
			Rollback(world, spawned.value);
			return FailRuntime<std::vector<WorldDocumentBinding>>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document transform was rejected");
		}

		*liveTransform = ToTransformComponent(entity.transform);
		if (!entity.model.empty())
		{
			if (world.Get<Renderable>(binding->entity) != nullptr)
			{
				Rollback(world, spawned.value);
				return FailRuntime<std::vector<WorldDocumentBinding>>(
					LogCategory::ECS,
					ErrorCode::InvalidArgument,
					"Scene document model was not loaded");
			}

			continue;
		}

		const SceneMeshRef* mesh = FindMesh(meshes, entity.mesh);
		Renderable* renderable = binding == nullptr ? nullptr : world.Get<Renderable>(binding->entity);
		if (binding == nullptr || mesh == nullptr || renderable == nullptr)
		{
			Rollback(world, spawned.value);
			return FailRuntime<std::vector<WorldDocumentBinding>>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document mesh key was not found");
		}

		renderable->mesh = mesh->mesh;
		renderable->material = render.material;
		renderable->overrides.baseColor = render.baseColor;
		if (render.materialConstantsSlot.has_value())
		{
			WorldMaterialParameterBlock block{};
			block.bindingSlot = *render.materialConstantsSlot;
			LitMaterialConstants constants{};
			std::memcpy(constants.tint, entity.tint, sizeof(constants.tint));
			const auto* bytes = reinterpret_cast<const std::byte*>(&constants);
			block.data.assign(bytes, bytes + sizeof(constants));
			renderable->overrides.parameters.push_back(std::move(block));
		}

		const bool bodyAdded = !entity.body.has_value() || world.Add<RigidBody>(binding->entity, ToRigidBody(*entity.body));
		const bool cameraAdded = !entity.camera.has_value() || world.Add<CameraComponent>(binding->entity, ToCameraComponent(*entity.camera));
		const bool lightAdded = !entity.light.has_value() || world.Add<LightComponent>(binding->entity, ToLightComponent(*entity.light));
		if (!bodyAdded || !cameraAdded || !lightAdded)
		{
			Rollback(world, spawned.value);
			return FailRuntime<std::vector<WorldDocumentBinding>>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document component was rejected");
		}
	}

	return spawned;
}

Result<std::string> WriteSceneDocument(const SceneDocument& document)
{
	if (!SameIds(document))
	{
		return FailRuntime<std::string>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document entities must match the world document");
	}

	for (size_t index = 0; index < document.world.size(); ++index)
	{
		const SceneEntity& entity = document.entities[index];
		const bool hasModel = !entity.model.empty();
		const bool hasMesh = !entity.mesh.empty();
		if (hasModel == hasMesh
			|| (hasModel && (document.world[index].hasRenderable || entity.body.has_value()))
			|| (entity.body.has_value() && document.world[index].parent != 0)
			|| (entity.body.has_value() && RigidBodySerializer::ShapeName(entity.body->shape.type) == nullptr)
			|| (entity.body.has_value() && RigidBodySerializer::MotionName(entity.body->motion) == nullptr))
		{
			return FailRuntime<std::string>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document entity cannot be written");
		}
	}

	Json entities = Json::array();
	for (size_t index = 0; index < document.world.size(); ++index)
	{
		entities.push_back(WriteEntity(document.world[index], document.entities[index]));
	}

	Json root = Json::object();
	root["version"] = kWorldDocumentVersion;
	root["entities"] = std::move(entities);

	try
	{
		return MakeOk(root.dump(2));
	}
	catch (const Json::exception&)
	{
		return FailRuntime<std::string>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document JSON could not be written");
	}
}
