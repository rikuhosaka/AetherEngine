#include "Engine/World/WorldDocument.h"

#include "ThirdParty/nlohmann/json.hpp"

#include <cstdint>
#include <string>
#include <string_view>
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

[[nodiscard]] bool ReadFloats(const Json& value, float* out, size_t count)
{
	if (!value.is_array() || value.size() != count)
	{
		return false;
	}

	for (size_t index = 0; index < count; ++index)
	{
		if (!value[index].is_number())
		{
			return false;
		}

		out[index] = value[index].get<float>();
	}

	return true;
}

[[nodiscard]] Result<WorldDocumentEntity> ReadEntity(const Json& value)
{
	if (!value.is_object())
	{
		return FailRuntime<WorldDocumentEntity>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"World document entity must be an object");
	}

	const Json* id = FindField(value, "id");
	const Json* name = FindField(value, "name");
	const Json* parent = FindField(value, "parent");
	const Json* transform = FindField(value, "transform");
	const Json* renderable = FindField(value, "renderable");
	if (id == nullptr || name == nullptr || parent == nullptr || transform == nullptr || renderable == nullptr
		|| !name->is_string() || !transform->is_object() || !renderable->is_object())
	{
		return FailRuntime<WorldDocumentEntity>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"World document entity is missing a field");
	}

	WorldDocumentEntity entity{};
	if (!ReadUint32(*id, entity.id))
	{
		return FailRuntime<WorldDocumentEntity>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"World document id must be an unsigned integer");
	}

	entity.name = name->get<std::string>();
	if (parent->is_null())
	{
		entity.parent = 0;
	}
	else if (!ReadUint32(*parent, entity.parent))
	{
		return FailRuntime<WorldDocumentEntity>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"World document parent must be null or an unsigned integer");
	}

	const Json* position = FindField(*transform, "position");
	const Json* rotation = FindField(*transform, "rotation");
	const Json* scale = FindField(*transform, "scale");
	if (position == nullptr || rotation == nullptr || scale == nullptr
		|| !ReadFloats(*position, &entity.transform.position.x, 3)
		|| !ReadFloats(*rotation, &entity.transform.rotation.x, 4)
		|| !ReadFloats(*scale, &entity.transform.scale.x, 3))
	{
		return FailRuntime<WorldDocumentEntity>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"World document transform is missing a field");
	}

	const Json* submesh = FindField(*renderable, "submesh");
	const Json* layerMask = FindField(*renderable, "layerMask");
	const Json* visible = FindField(*renderable, "visible");
	uint32_t layerValue = 0;
	if (submesh == nullptr || layerMask == nullptr || visible == nullptr || !visible->is_boolean()
		|| !ReadUint32(*submesh, entity.submeshIndex) || !ReadUint32(*layerMask, layerValue) || layerValue > 255u)
	{
		return FailRuntime<WorldDocumentEntity>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"World document renderable is missing a field");
	}

	entity.layerMask = static_cast<uint8_t>(layerValue);
	entity.visible = visible->get<bool>();
	return MakeOk(std::move(entity));
}

[[nodiscard]] Result<WorldDocument> ReadDocument(std::string_view text)
{
	const Json root = Json::parse(text.begin(), text.end(), nullptr, false);
	if (root.is_discarded() || !root.is_object())
	{
		return FailRuntime<WorldDocument>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"World document JSON is invalid");
	}

	const Json* version = FindField(root, "version");
	const Json* entities = FindField(root, "entities");
	uint32_t versionValue = 0;
	if (version == nullptr || entities == nullptr || !entities->is_array() || !ReadUint32(*version, versionValue))
	{
		return FailRuntime<WorldDocument>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"World document JSON is missing version or entities");
	}

	if (versionValue != static_cast<uint32_t>(kWorldDocumentVersion))
	{
		return FailRuntime<WorldDocument>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"World document version is unsupported");
	}

	WorldDocument document{};
	document.reserve(entities->size());
	for (const Json& entityValue : *entities)
	{
		Result<WorldDocumentEntity> entity = ReadEntity(entityValue);
		if (!entity)
		{
			return MakeFail<WorldDocument>(entity.error.code, entity.error.message);
		}

		document.push_back(std::move(entity.value));
	}

	return MakeOk(std::move(document));
}
} // namespace

Result<std::string> WriteWorldDocument(const WorldDocument& document)
{
	Json entities = Json::array();
	for (const WorldDocumentEntity& entity : document)
	{
		Json item = Json::object();
		item["id"] = entity.id;
		item["name"] = entity.name;
		item["parent"] = entity.parent == 0 ? Json(nullptr) : Json(entity.parent);
		item["transform"] = Json::object({
			{ "position", Json::array({ entity.transform.position.x, entity.transform.position.y, entity.transform.position.z }) },
			{ "rotation", Json::array({
				entity.transform.rotation.x,
				entity.transform.rotation.y,
				entity.transform.rotation.z,
				entity.transform.rotation.w }) },
			{ "scale", Json::array({ entity.transform.scale.x, entity.transform.scale.y, entity.transform.scale.z }) },
		});
		item["renderable"] = Json::object({
			{ "submesh", entity.submeshIndex },
			{ "layerMask", static_cast<uint32_t>(entity.layerMask) },
			{ "visible", entity.visible },
		});
		entities.push_back(std::move(item));
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
			"World document JSON could not be written");
	}
}

Result<WorldDocument> ReadWorldDocument(std::string_view text)
{
	try
	{
		return ReadDocument(text);
	}
	catch (const Json::exception&)
	{
		return FailRuntime<WorldDocument>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"World document JSON is invalid");
	}
}
