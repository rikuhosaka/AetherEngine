#include "Runtime/Engine/SceneDocument/Serializer/RigidBodySerializer.h"

#include "Runtime/Engine/SceneDocument/Serializer/JsonFields.h"

#include <string>
#include <utility>

namespace
{
using Json = nlohmann::json;

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

[[nodiscard]] Result<RigidBodyShape> ReadShape(const Json& value)
{
	if (!value.is_object())
	{
		return FailRuntime<RigidBodyShape>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document shape must be an object");
	}

	const Json* typeValue = SceneJson::FindField(value, "type");
	if (typeValue == nullptr || !typeValue->is_string())
	{
		return FailRuntime<RigidBodyShape>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document shape is missing a type");
	}

	const std::string type = typeValue->get<std::string>();
	RigidBodyShape shape{};
	if (type == "box")
	{
		shape.type = PhysicsShapeType::Box;
		const Json* halfExtent = SceneJson::FindField(value, "halfExtent");
		if (halfExtent == nullptr || !SceneJson::ReadFloats(*halfExtent, &shape.halfExtent.x, 3))
		{
			return FailRuntime<RigidBodyShape>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document box is missing halfExtent");
		}
	}
	else if (type == "sphere")
	{
		shape.type = PhysicsShapeType::Sphere;
		const Json* radius = SceneJson::FindField(value, "radius");
		if (radius == nullptr || !SceneJson::ReadFloat(*radius, shape.radius))
		{
			return FailRuntime<RigidBodyShape>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document sphere is missing radius");
		}
	}
	else if (type == "capsule")
	{
		shape.type = PhysicsShapeType::Capsule;
		const Json* radius = SceneJson::FindField(value, "radius");
		const Json* halfHeight = SceneJson::FindField(value, "halfHeight");
		if (radius == nullptr || halfHeight == nullptr || !SceneJson::ReadFloat(*radius, shape.radius)
			|| !SceneJson::ReadFloat(*halfHeight, shape.halfHeight))
		{
			return FailRuntime<RigidBodyShape>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document capsule is missing radius or halfHeight");
		}
	}
	else if (type == "convexHull" || type == "mesh")
	{
		shape.type = type == "convexHull" ? PhysicsShapeType::ConvexHull : PhysicsShapeType::Mesh;
		if (SceneJson::FindField(value, "points") != nullptr || SceneJson::FindField(value, "indices") != nullptr)
		{
			return FailRuntime<RigidBodyShape>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document mesh asset must not embed points or indices");
		}

		const Json* mesh = SceneJson::FindField(value, "mesh");
		if (mesh == nullptr || !mesh->is_string() || mesh->get<std::string>().empty())
		{
			return FailRuntime<RigidBodyShape>(
				LogCategory::ECS,
				ErrorCode::InvalidArgument,
				"Scene document shape is missing a mesh asset");
		}

		shape.mesh = mesh->get<std::string>();
	}
	else
	{
		return FailRuntime<RigidBodyShape>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document shape is unsupported");
	}

	return MakeOk(std::move(shape));
}

[[nodiscard]] Json WriteShape(const RigidBodyShape& shape)
{
	Json value = Json::object();
	value["type"] = RigidBodySerializer::ShapeName(shape.type);
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
		value["mesh"] = shape.mesh;
		break;
	default:
		break;
	}

	return value;
}
} // namespace

const char* RigidBodySerializer::MotionName(PhysicsMotion motion)
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

const char* RigidBodySerializer::ShapeName(PhysicsShapeType type)
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

Result<RigidBody> RigidBodySerializer::Read(const nlohmann::json& value)
{
	if (!value.is_object())
	{
		return FailRuntime<RigidBody>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document physics must be an object");
	}

	const nlohmann::json* motion = SceneJson::FindField(value, "motion");
	const nlohmann::json* friction = SceneJson::FindField(value, "friction");
	const nlohmann::json* restitution = SceneJson::FindField(value, "restitution");
	const nlohmann::json* mass = SceneJson::FindField(value, "mass");
	const nlohmann::json* shape = SceneJson::FindField(value, "shape");
	RigidBody body{};
	if (motion == nullptr || !ReadMotion(*motion, body.motion))
	{
		return FailRuntime<RigidBody>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			motion != nullptr && motion->is_string()
				? "Scene document motion is unsupported"
				: "Scene document physics is missing a field");
	}

	if (friction == nullptr || restitution == nullptr || mass == nullptr || shape == nullptr
		|| !SceneJson::ReadFloat(*friction, body.friction) || !SceneJson::ReadFloat(*restitution, body.restitution)
		|| !SceneJson::ReadFloat(*mass, body.mass))
	{
		return FailRuntime<RigidBody>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document physics is missing a field");
	}

	Result<RigidBodyShape> readShape = ReadShape(*shape);
	if (!readShape)
	{
		return MakeFail<RigidBody>(readShape.error.code, readShape.error.message);
	}

	body.shape = std::move(readShape.value);
	if (body.shape.type == PhysicsShapeType::Mesh && body.motion != PhysicsMotion::Static)
	{
		return FailRuntime<RigidBody>(
			LogCategory::ECS,
			ErrorCode::InvalidArgument,
			"Scene document mesh physics requires static motion");
	}

	return MakeOk(std::move(body));
}

nlohmann::json RigidBodySerializer::Write(const RigidBody& body)
{
	return nlohmann::json::object({
		{ "motion", MotionName(body.motion) },
		{ "friction", body.friction },
		{ "restitution", body.restitution },
		{ "mass", body.mass },
		{ "shape", WriteShape(body.shape) },
	});
}
