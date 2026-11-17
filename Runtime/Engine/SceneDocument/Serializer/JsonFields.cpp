#include "Runtime/Engine/SceneDocument/Serializer/JsonFields.h"

#include <string>

namespace SceneJson
{
const nlohmann::json* FindField(const nlohmann::json& object, std::string_view key)
{
	if (!object.is_object())
	{
		return nullptr;
	}

	const nlohmann::json::const_iterator field = object.find(std::string(key));
	if (field == object.end())
	{
		return nullptr;
	}

	return &(*field);
}

bool ReadFloat(const nlohmann::json& value, float& out)
{
	if (!value.is_number())
	{
		return false;
	}

	out = value.get<float>();
	return true;
}

bool ReadFloats(const nlohmann::json& value, float* out, size_t count)
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
} // namespace SceneJson
