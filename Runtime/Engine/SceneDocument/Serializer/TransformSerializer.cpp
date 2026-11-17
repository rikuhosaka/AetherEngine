#include "Runtime/Engine/SceneDocument/Serializer/TransformSerializer.h"

#include "Runtime/Engine/SceneDocument/Serializer/JsonFields.h"

namespace
{
using Json = nlohmann::json;
} // namespace

Result<TransformComponent> TransformSerializer::Read(const nlohmann::json& value)
{
	if (!value.is_object())
	{
		return FailRuntime<TransformComponent>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document transform is missing a field");
	}

	const Json* position = SceneJson::FindField(value, "position");
	const Json* rotation = SceneJson::FindField(value, "rotation");
	const Json* scale = SceneJson::FindField(value, "scale");
	TransformComponent transform{};
	if (position == nullptr || rotation == nullptr || scale == nullptr
		|| !SceneJson::ReadFloats(*position, &transform.position.x, 3)
		|| !SceneJson::ReadFloats(*rotation, &transform.rotation.x, 4)
		|| !SceneJson::ReadFloats(*scale, &transform.scale.x, 3))
	{
		return FailRuntime<TransformComponent>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document transform is missing a field");
	}

	return MakeOk(transform);
}

nlohmann::json TransformSerializer::Write(const TransformComponent& transform)
{
	return nlohmann::json::object({
		{ "position", nlohmann::json::array({ transform.position.x, transform.position.y, transform.position.z }) },
		{ "rotation", nlohmann::json::array({
			transform.rotation.x,
			transform.rotation.y,
			transform.rotation.z,
			transform.rotation.w }) },
		{ "scale", nlohmann::json::array({ transform.scale.x, transform.scale.y, transform.scale.z }) },
	});
}
