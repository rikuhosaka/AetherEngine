#include "Runtime/Engine/SceneDocument/Serializer/CameraComponentSerializer.h"

#include "Runtime/Core/Json/JsonFields.h"

namespace
{
using Json = nlohmann::json;
} // namespace

Result<SceneCamera> CameraComponentSerializer::Read(const nlohmann::json& value)
{
	if (!value.is_object())
	{
		return FailRuntime<SceneCamera>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document camera is invalid");
	}

	const Json* fovY = JsonFields::FindField(value, "fovY");
	const Json* nearPlane = JsonFields::FindField(value, "near");
	const Json* farPlane = JsonFields::FindField(value, "far");
	SceneCamera component{};
	if (fovY == nullptr || nearPlane == nullptr || farPlane == nullptr
		|| !JsonFields::ReadFloat(*fovY, component.fovYDegrees) || !JsonFields::ReadFloat(*nearPlane, component.nearPlane)
		|| !JsonFields::ReadFloat(*farPlane, component.farPlane))
	{
		return FailRuntime<SceneCamera>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document camera is invalid");
	}

	return MakeOk(component);
}

nlohmann::json CameraComponentSerializer::Write(const SceneCamera& camera)
{
	return nlohmann::json::object({
		{ "fovY", camera.fovYDegrees },
		{ "near", camera.nearPlane },
		{ "far", camera.farPlane },
	});
}
