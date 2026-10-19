#include "Engine/Scene/SceneDocument.h"

#include "Engine/World/World.h"
#include "ThirdParty/nlohmann/json.hpp"

#include <cstddef>
#include <utility>

namespace
{
using Json = nlohmann::json;

[[nodiscard]] const Json* FindField(const Json& object, const char* key)
{
	if (!object.is_object())
	{
		return nullptr;
	}

	const Json::const_iterator field = object.find(key);
	if (field == object.end())
	{
		return nullptr;
	}

	return &(*field);
}

[[nodiscard]] bool ReadUint32(const Json& value, uint32_t& out)
{
	if (!value.is_number_unsigned())
	{
		return false;
	}

	const uint64_t number = value.get<uint64_t>();
	if (number > UINT32_MAX)
	{
		return false;
	}

	out = static_cast<uint32_t>(number);
	return true;
}

[[nodiscard]] bool ReadFloat(const Json& value, float& out)
{
	if (!value.is_number())
	{
		return false;
	}

	out = value.get<float>();
	return true;
}

[[nodiscard]] bool ReadFloats(const Json& value, float* out, size_t count)
{
	if (!value.is_array() || value.size() != count)
	{
		return false;
	}

	for (size_t index = 0; index < count; ++index)
	{
		if (!ReadFloat(value[index], out[index]))
		{
			return false;
		}
	}

	return true;
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

[[nodiscard]] bool ReadMotion(const Json& value, PhysicsMotion& out)
{
	if (!value.is_string())
	{
		return false;
	}

	const std::string motion = value.get<std::string>();
	if (motion == "static")
	{
		out = PhysicsMotion::Static;
		return true;
	}
	if (motion == "dynamic")
	{
		out = PhysicsMotion::Dynamic;
		return true;
	}
	if (motion == "kinematic")
	{
		out = PhysicsMotion::Kinematic;
		return true;
	}

	return false;
}

[[nodiscard]] Result<std::vector<DirectX::XMFLOAT3>> ReadPoints(const Json& value)
{
	if (!value.is_array())
	{
		return FailRuntime<std::vector<DirectX::XMFLOAT3>>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document shape is missing points");
	}

	std::vector<DirectX::XMFLOAT3> points{};
	points.reserve(value.size());
	for (const Json& pointValue : value)
	{
		DirectX::XMFLOAT3 point{};
		if (!ReadFloats(pointValue, &point.x, 3))
		{
			return FailRuntime<std::vector<DirectX::XMFLOAT3>>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document shape point must have three components");
		}

		points.push_back(point);
	}

	return MakeOk(std::move(points));
}

[[nodiscard]] Result<SceneShape> ReadShape(const Json& value)
{
	if (!value.is_object())
	{
		return FailRuntime<SceneShape>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document shape must be an object");
	}

	const Json* typeValue = FindField(value, "type");
	if (typeValue == nullptr || !typeValue->is_string())
	{
		return FailRuntime<SceneShape>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document shape is missing a type");
	}

	const std::string type = typeValue->get<std::string>();
	SceneShape shape{};
	if (type == "box")
	{
		shape.type = PhysicsShapeType::Box;
		const Json* halfExtent = FindField(value, "halfExtent");
		if (halfExtent == nullptr || !ReadFloats(*halfExtent, &shape.halfExtent.x, 3))
		{
			return FailRuntime<SceneShape>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document box is missing halfExtent");
		}
	}
	else if (type == "sphere")
	{
		shape.type = PhysicsShapeType::Sphere;
		const Json* radius = FindField(value, "radius");
		if (radius == nullptr || !ReadFloat(*radius, shape.radius))
		{
			return FailRuntime<SceneShape>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document sphere is missing radius");
		}
	}
	else if (type == "capsule")
	{
		shape.type = PhysicsShapeType::Capsule;
		const Json* radius = FindField(value, "radius");
		const Json* halfHeight = FindField(value, "halfHeight");
		if (radius == nullptr || halfHeight == nullptr || !ReadFloat(*radius, shape.radius)
			|| !ReadFloat(*halfHeight, shape.halfHeight))
		{
			return FailRuntime<SceneShape>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document capsule is missing radius or halfHeight");
		}
	}
	else if (type == "convexHull" || type == "mesh")
	{
		shape.type = type == "convexHull" ? PhysicsShapeType::ConvexHull : PhysicsShapeType::Mesh;
		const Json* points = FindField(value, "points");
		if (points == nullptr)
		{
			return FailRuntime<SceneShape>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document shape is missing points");
		}

		Result<std::vector<DirectX::XMFLOAT3>> readPoints = ReadPoints(*points);
		if (!readPoints)
		{
			return MakeFail<SceneShape>(readPoints.error.code, readPoints.error.message);
		}

		shape.points = std::move(readPoints.value);
		if (shape.type == PhysicsShapeType::ConvexHull && shape.points.size() < 4)
		{
			return FailRuntime<SceneShape>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document convex hull requires at least four points");
		}

		if (shape.type == PhysicsShapeType::Mesh)
		{
			const Json* indices = FindField(value, "indices");
			if (indices == nullptr || !indices->is_array() || indices->empty() || indices->size() % 3 != 0)
			{
				return FailRuntime<SceneShape>(
					LogCategory::ECS,
					ErrorCode::InvalidArgument,
					"Scene document mesh is missing triangle indices");
			}

			shape.indices.reserve(indices->size());
			for (const Json& indexValue : *indices)
			{
				uint32_t index = 0;
				if (!ReadUint32(indexValue, index))
				{
					return FailRuntime<SceneShape>(
						LogCategory::ECS,
						ErrorCode::InvalidArgument,
						"Scene document mesh index must be an unsigned integer");
				}

				shape.indices.push_back(index);
			}
		}
	}
	else
	{
		return FailRuntime<SceneShape>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document shape is unsupported");
	}

	return MakeOk(std::move(shape));
}

[[nodiscard]] Result<SceneBody> ReadBody(const Json& value)
{
	if (!value.is_object())
	{
		return FailRuntime<SceneBody>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document physics must be an object");
	}

	const Json* motion = FindField(value, "motion");
	const Json* friction = FindField(value, "friction");
	const Json* restitution = FindField(value, "restitution");
	const Json* mass = FindField(value, "mass");
	const Json* shape = FindField(value, "shape");
	SceneBody body{};
	if (motion == nullptr || !ReadMotion(*motion, body.motion))
	{
		return FailRuntime<SceneBody>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			motion != nullptr && motion->is_string()
				? "Scene document motion is unsupported"
				: "Scene document physics is missing a field");
	}

	if (friction == nullptr || restitution == nullptr || mass == nullptr || shape == nullptr
		|| !ReadFloat(*friction, body.friction) || !ReadFloat(*restitution, body.restitution)
		|| !ReadFloat(*mass, body.mass))
	{
		return FailRuntime<SceneBody>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document physics is missing a field");
	}

	Result<SceneShape> readShape = ReadShape(*shape);
	if (!readShape)
	{
		return MakeFail<SceneBody>(readShape.error.code, readShape.error.message);
	}

	body.shape = std::move(readShape.value);
	if (body.shape.type == PhysicsShapeType::Mesh && body.motion != PhysicsMotion::Static)
	{
		return FailRuntime<SceneBody>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document mesh physics requires static motion");
	}

	return MakeOk(std::move(body));
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

	const Json* renderable = FindField(value, "renderable");
	if (renderable == nullptr || !renderable->is_object())
	{
		return FailRuntime<SceneEntity>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document renderable is missing");
	}

	const Json* mesh = FindField(*renderable, "mesh");
	const Json* tint = FindField(*renderable, "tint");
	SceneEntity entity{};
	entity.id = worldEntity.id;
	if (mesh == nullptr || !mesh->is_string() || mesh->get<std::string>().empty() || tint == nullptr
		|| !ReadFloats(*tint, entity.tint, 4))
	{
		return FailRuntime<SceneEntity>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document mesh or tint is missing");
	}

	entity.mesh = mesh->get<std::string>();
	if (const Json* physics = FindField(value, "physics"))
	{
		if (worldEntity.parent != 0)
		{
			return FailRuntime<SceneEntity>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document physics requires a root entity");
		}

		Result<SceneBody> body = ReadBody(*physics);
		if (!body)
		{
			return MakeFail<SceneEntity>(body.error.code, body.error.message);
		}

		entity.body = std::move(body.value);
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
	const Json* entities = root.is_discarded() ? nullptr : FindField(root, "entities");
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

[[nodiscard]] PhysicsBodyDesc MakeBodyDesc(const SceneBody& body)
{
	PhysicsBodyDesc desc{};
	desc.motion = body.motion;
	desc.friction = body.friction;
	desc.restitution = body.restitution;
	desc.mass = body.mass;
	desc.shape.type = body.shape.type;
	desc.shape.halfExtent = body.shape.halfExtent;
	desc.shape.radius = body.shape.radius;
	desc.shape.halfHeight = body.shape.halfHeight;
	desc.shape.points = body.shape.points;
	desc.shape.indices = body.shape.indices;
	return desc;
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

[[nodiscard]] const char* MotionName(PhysicsMotion motion)
{
	switch (motion)
	{
	case PhysicsMotion::Static:
		return "static";
	case PhysicsMotion::Dynamic:
		return "dynamic";
	case PhysicsMotion::Kinematic:
		return "kinematic";
	default:
		return nullptr;
	}
}

[[nodiscard]] const char* ShapeName(PhysicsShapeType type)
{
	switch (type)
	{
	case PhysicsShapeType::Box:
		return "box";
	case PhysicsShapeType::Sphere:
		return "sphere";
	case PhysicsShapeType::Capsule:
		return "capsule";
	case PhysicsShapeType::ConvexHull:
		return "convexHull";
	case PhysicsShapeType::Mesh:
		return "mesh";
	default:
		return nullptr;
	}
}

[[nodiscard]] Json WriteShape(const SceneShape& shape)
{
	Json value = Json::object();
	value["type"] = ShapeName(shape.type);
	switch (shape.type)
	{
	case PhysicsShapeType::Box:
		value["halfExtent"] = Json::array({ shape.halfExtent.x, shape.halfExtent.y, shape.halfExtent.z });
		break;
	case PhysicsShapeType::Sphere:
		value["radius"] = shape.radius;
		break;
	case PhysicsShapeType::Capsule:
		value["radius"] = shape.radius;
		value["halfHeight"] = shape.halfHeight;
		break;
	case PhysicsShapeType::ConvexHull:
	case PhysicsShapeType::Mesh:
	{
		Json points = Json::array();
		for (const DirectX::XMFLOAT3& point : shape.points)
		{
			points.push_back(Json::array({ point.x, point.y, point.z }));
		}
		value["points"] = std::move(points);
		if (shape.type == PhysicsShapeType::Mesh)
		{
			value["indices"] = shape.indices;
		}
		break;
	}
	default:
		break;
	}

	return value;
}

[[nodiscard]] Json WriteEntity(const WorldDocumentEntity& worldEntity, const SceneEntity& entity)
{
	Json item = Json::object();
	item["id"] = worldEntity.id;
	item["name"] = worldEntity.name;
	item["parent"] = worldEntity.parent == 0 ? Json(nullptr) : Json(worldEntity.parent);
	item["transform"] = Json::object({
		{ "position", Json::array({
			worldEntity.transform.position.x,
			worldEntity.transform.position.y,
			worldEntity.transform.position.z }) },
		{ "rotation", Json::array({
			worldEntity.transform.rotation.x,
			worldEntity.transform.rotation.y,
			worldEntity.transform.rotation.z,
			worldEntity.transform.rotation.w }) },
		{ "scale", Json::array({
			worldEntity.transform.scale.x,
			worldEntity.transform.scale.y,
			worldEntity.transform.scale.z }) },
	});
	item["renderable"] = Json::object({
		{ "mesh", entity.mesh },
		{ "submesh", worldEntity.submeshIndex },
		{ "layerMask", static_cast<uint32_t>(worldEntity.layerMask) },
		{ "visible", worldEntity.visible },
		{ "tint", Json::array({ entity.tint[0], entity.tint[1], entity.tint[2], entity.tint[3] }) },
	});
	if (entity.body.has_value())
	{
		item["physics"] = Json::object({
			{ "motion", MotionName(entity.body->motion) },
			{ "friction", entity.body->friction },
			{ "restitution", entity.body->restitution },
			{ "mass", entity.body->mass },
			{ "shape", WriteShape(entity.body->shape) },
		});
	}

	return item;
}

void Rollback(
	World& world,
	EntityPhysics& physics,
	const std::vector<WorldDocumentBinding>& spawned,
	const std::vector<EntityId>& bound)
{
	for (const EntityId id : bound)
	{
		(void)physics.Unbind(id);
	}

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

Result<std::vector<WorldDocumentBinding>> InstantiateScene(
	World& world,
	EntityPhysics& physics,
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

	std::vector<EntityId> bound{};
	for (size_t index = 0; index < document.entities.size(); ++index)
	{
		const SceneEntity& entity = document.entities[index];
		const WorldDocumentBinding* binding = FindBinding(spawned.value, entity.id);
		const SceneMeshRef* mesh = FindMesh(meshes, entity.mesh);
		Renderable* renderable = binding == nullptr ? nullptr : world.GetRenderable(binding->entity);
		if (binding == nullptr || mesh == nullptr || renderable == nullptr)
		{
			Rollback(world, physics, spawned.value, bound);
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

		if (!entity.body.has_value())
		{
			continue;
		}

		const Result<PhysicsBodyId> body = physics.Bind(world, binding->entity, MakeBodyDesc(*entity.body));
		if (!body)
		{
			Rollback(world, physics, spawned.value, bound);
			return MakeFail<std::vector<WorldDocumentBinding>>(body.error.code, body.error.message);
		}

		bound.push_back(binding->entity);
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
		if (entity.mesh.empty() || (entity.body.has_value() && document.world[index].parent != 0)
			|| (entity.body.has_value() && ShapeName(entity.body->shape.type) == nullptr)
			|| (entity.body.has_value() && MotionName(entity.body->motion) == nullptr))
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
