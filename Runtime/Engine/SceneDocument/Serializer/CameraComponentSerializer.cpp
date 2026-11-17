#include "Runtime/Engine/SceneDocument/Serializer/CameraComponentSerializer.h"

#include "Runtime/Engine/SceneDocument/Serializer/JsonFields.h"

namespace
{
using Json = nlohmann::json;
} // namespace

Result<CameraComponent> CameraComponentSerializer::Read(const nlohmann::json& value)
{
	if (!value.is_object())
	{
		return FailRuntime<CameraComponent>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document camera is invalid");
	}

	const Json* fovY = SceneJson::FindField(value, "fovY");
	const Json* nearPlane = SceneJson::FindField(value, "near");
	const Json* farPlane = SceneJson::FindField(value, "far");
	CameraComponent component{};
	if (fovY == nullptr || nearPlane == nullptr || farPlane == nullptr
		|| !SceneJson::ReadFloat(*fovY, component.fovYDegrees) || !SceneJson::ReadFloat(*nearPlane, component.nearPlane)
		|| !SceneJson::ReadFloat(*farPlane, component.farPlane))
	{
		return FailRuntime<CameraComponent>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document camera is invalid");
	}

	return MakeOk(component);
}

nlohmann::json CameraComponentSerializer::Write(const CameraComponent& camera)
{
	return nlohmann::json::object({
		{ "fovY", camera.fovYDegrees },
		{ "near", camera.nearPlane },
		{ "far", camera.farPlane },
	});
}
