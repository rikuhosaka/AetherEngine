#include "Runtime/Engine/SceneDocument/Serializer/LightComponentSerializer.h"

#include "Runtime/Core/Json/JsonFields.h"

namespace
{
using Json = nlohmann::json;
} // namespace

Result<LightComponent> LightComponentSerializer::Read(const nlohmann::json& value)
{
	if (!value.is_object())
	{
		return FailRuntime<LightComponent>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document light is invalid");
	}

	const Json* color = JsonFields::FindField(value, "color");
	const Json* intensity = JsonFields::FindField(value, "intensity");
	LightComponent component{};
	if (color == nullptr || intensity == nullptr || !JsonFields::ReadFloats(*color, component.color, 3)
		|| !JsonFields::ReadFloat(*intensity, component.intensity))
	{
		return FailRuntime<LightComponent>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document light is invalid");
	}

	return MakeOk(component);
}

nlohmann::json LightComponentSerializer::Write(const LightComponent& light)
{
	return nlohmann::json::object({
		{ "color", nlohmann::json::array({ light.color[0], light.color[1], light.color[2] }) },
		{ "intensity", light.intensity },
	});
}
