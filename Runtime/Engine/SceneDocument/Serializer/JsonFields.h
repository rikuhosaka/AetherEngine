#pragma once

#include "ThirdParty/nlohmann/json.hpp"

#include <cstddef>
#include <string_view>

namespace SceneJson
{
[[nodiscard]] const nlohmann::json* FindField(const nlohmann::json& object, std::string_view key);
[[nodiscard]] bool ReadFloat(const nlohmann::json& value, float& out);
[[nodiscard]] bool ReadFloats(const nlohmann::json& value, float* out, size_t count);
} // namespace SceneJson
