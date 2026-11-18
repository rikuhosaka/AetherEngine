#pragma once

#include "ThirdParty/nlohmann/json.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace JsonFields
{
[[nodiscard]] inline const nlohmann::json* FindField(const nlohmann::json& object, std::string_view key)
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

[[nodiscard]] inline bool ReadFloat(const nlohmann::json& value, float& out)
{
	if (!value.is_number())
	{
		return false;
	}

	out = value.get<float>();
	return std::isfinite(out);
}

[[nodiscard]] inline bool ReadFloats(const nlohmann::json& value, float* out, size_t count)
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

[[nodiscard]] inline bool ReadUint32(const nlohmann::json& value, uint32_t& out)
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
} // namespace JsonFields
