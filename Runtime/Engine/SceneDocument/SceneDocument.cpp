#include "Runtime/Engine/SceneDocument/SceneDocument.h"

#include "Runtime/Engine/SceneDocument/Serializer/CameraComponentSerializer.h"
#include "Runtime/Engine/SceneDocument/Serializer/JsonFields.h"
#include "Runtime/Engine/SceneDocument/Serializer/LightComponentSerializer.h"
#include "Runtime/Engine/SceneDocument/Serializer/RigidBodySerializer.h"
#include "Runtime/Engine/SceneDocument/Serializer/TransformSerializer.h"
#include "Runtime/Engine/World/World.h"
#include "ThirdParty/nlohmann/json.hpp"

#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>

namespace
{
using Json = nlohmann::json;

[[nodiscard]] bool IsFbxModelPath(const std::filesystem::path& path)
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

	return extension == ".fbx";
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

	const Json* renderable = SceneJson::FindField(value, "renderable");
	const Json* model = SceneJson::FindField(value, "model");
	SceneEntity entity{};
	entity.id = worldEntity.id;
	const Json* transformValue = SceneJson::FindField(value, TransformSerializer::kKey);
	if (transformValue == nullptr)
	{
		return FailRuntime<SceneEntity>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document transform is missing a field");
	}

	Result<TransformComponent> transform = TransformSerializer::Read(*transformValue);
	if (!transform)
	{
		return MakeFail<SceneEntity>(transform.error.code, transform.error.message);
	}

	entity.transform = std::move(transform.value);
	if (model != nullptr)
	{
		if (!model->is_string() || !IsFbxModelPath(model->get<std::string>()) || renderable != nullptr
			|| worldEntity.hasRenderable)
		{
			return FailRuntime<SceneEntity>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document model must be an FBX path without a renderable");
		}

		entity.model = model->get<std::string>();
		if (SceneJson::FindField(value, RigidBodySerializer::kKey) != nullptr)
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

		const Json* mesh = SceneJson::FindField(*renderable, "mesh");
		const Json* tint = SceneJson::FindField(*renderable, "tint");
		if (mesh == nullptr || !mesh->is_string() || mesh->get<std::string>().empty() || tint == nullptr
			|| !SceneJson::ReadFloats(*tint, entity.tint, 4))
		{
			return FailRuntime<SceneEntity>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document mesh or tint is missing");
		}

		entity.mesh = mesh->get<std::string>();
	}
	if (const Json* physics = SceneJson::FindField(value, RigidBodySerializer::kKey))
	{
		if (worldEntity.parent != 0)
		{
			return FailRuntime<SceneEntity>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document physics requires a root entity");
		}

		Result<RigidBody> body = RigidBodySerializer::Read(*physics);
		if (!body)
		{
			return MakeFail<SceneEntity>(body.error.code, body.error.message);
		}

		entity.body = std::move(body.value);
	}

	if (const Json* camera = SceneJson::FindField(value, CameraComponentSerializer::kKey))
	{
		Result<CameraComponent> component = CameraComponentSerializer::Read(*camera);
		if (!component)
		{
			return MakeFail<SceneEntity>(component.error.code, component.error.message);
		}

		entity.camera = std::move(component.value);
	}

	if (const Json* light = SceneJson::FindField(value, LightComponentSerializer::kKey))
	{
		Result<LightComponent> component = LightComponentSerializer::Read(*light);
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
	const Json* entities = root.is_discarded() ? nullptr : SceneJson::FindField(root, "entities");
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

[[nodiscard]] const SceneModelRef* FindModel(std::span<const SceneModelRef> models, uint32_t documentId)
{
	for (const SceneModelRef& model : models)
	{
		if (model.documentId == documentId)
		{
			return &model;
		}
	}

	return nullptr;
}

Result<std::vector<WorldDocumentBinding>> InstantiateScene(
	World& world,
	const SceneDocument& document,
	std::span<const SceneMeshRef> meshes,
	const SceneRenderResources& render,
	std::span<const SceneModelRef> models)
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

		liveTransform->position = entity.transform.position;
		liveTransform->rotation = entity.transform.rotation;
		liveTransform->scale = entity.transform.scale;
		if (!entity.model.empty())
		{
			const SceneModelRef* model = FindModel(models, entity.id);
			if (binding == nullptr || model == nullptr || model->assets == nullptr
				|| world.Get<Renderable>(binding->entity) != nullptr)
			{
				Rollback(world, spawned.value);
				return FailRuntime<std::vector<WorldDocumentBinding>>(
					LogCategory::ECS,
					ErrorCode::InvalidArgument,
					"Scene document model was not loaded");
			}

			if (auto spawnedModel = model->assets->SpawnUnder(world, binding->entity); !spawnedModel)
			{
				Rollback(world, spawned.value);
				return MakeFail<std::vector<WorldDocumentBinding>>(spawnedModel.error.code, spawnedModel.error.message);
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
			const auto* bytes = reinterpret_cast<const std::byte*>(entity.tint);
			block.data.assign(bytes, bytes + sizeof(entity.tint));
			renderable->overrides.parameters.push_back(std::move(block));
		}

		const bool bodyAdded = !entity.body.has_value() || world.Add<RigidBody>(binding->entity, *entity.body);
		const bool cameraAdded = !entity.camera.has_value() || world.Add<CameraComponent>(binding->entity, *entity.camera);
		const bool lightAdded = !entity.light.has_value() || world.Add<LightComponent>(binding->entity, *entity.light);
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
