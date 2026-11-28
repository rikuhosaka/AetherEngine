#include "Runtime/Engine/SceneDocument/Serializer/TransformSerializer.h"

#include "Runtime/Core/Json/JsonFields.h"

namespace
{
using Json = nlohmann::json;
} // namespace

Result<SceneTransform> TransformSerializer::Read(const nlohmann::json& value)
{
	if (!value.is_object())
	{
		return FailRuntime<SceneTransform>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document transform is missing a field");
	}

	const Json* position = JsonFields::FindField(value, "position");
	const Json* rotation = JsonFields::FindField(value, "rotation");
	const Json* scale = JsonFields::FindField(value, "scale");
	SceneTransform transform{};
	if (position == nullptr || rotation == nullptr || scale == nullptr
		|| !JsonFields::ReadFloats(*position, &transform.position.x, 3)
		|| !JsonFields::ReadFloats(*rotation, &transform.rotation.x, 4)
		|| !JsonFields::ReadFloats(*scale, &transform.scale.x, 3))
	{
		return FailRuntime<SceneTransform>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document transform is missing a field");
	}

	return MakeOk(transform);
}

nlohmann::json TransformSerializer::Write(const SceneTransform& transform)
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
