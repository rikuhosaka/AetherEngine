#include "Runtime/Engine/SceneDocument/Serializer/LightComponentSerializer.h"

#include "Runtime/Core/Json/JsonFields.h"

namespace
{
using Json = nlohmann::json;
} // namespace

Result<SceneLight> LightComponentSerializer::Read(const nlohmann::json& value)
{
	if (!value.is_object())
	{
		return FailRuntime<SceneLight>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document light is invalid");
	}

	const Json* color = JsonFields::FindField(value, "color");
	const Json* intensity = JsonFields::FindField(value, "intensity");
	SceneLight component{};
	if (color == nullptr || intensity == nullptr || !JsonFields::ReadFloats(*color, component.color, 3)
		|| !JsonFields::ReadFloat(*intensity, component.intensity))
	{
		return FailRuntime<SceneLight>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document light is invalid");
	}

	return MakeOk(component);
}

nlohmann::json LightComponentSerializer::Write(const SceneLight& light)
{
	return nlohmann::json::object({
		{ "color", nlohmann::json::array({ light.color[0], light.color[1], light.color[2] }) },
		{ "intensity", light.intensity },
	});
}
