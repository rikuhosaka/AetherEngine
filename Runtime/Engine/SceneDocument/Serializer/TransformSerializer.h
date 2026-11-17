#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/World/Component/TransformComponent.h"
#include "ThirdParty/nlohmann/json.hpp"

#include <string_view>

// 責任: 文書上の姿勢と transform オブジェクトの変換を持つ。
// 役割: 位置、回転、拡縮を読み書きする。
// 所有: 状態を持たない。
class TransformSerializer
{
public:
	static constexpr std::string_view kKey = "transform";

	[[nodiscard]] static Result<TransformComponent> Read(const nlohmann::json& value);
	[[nodiscard]] static nlohmann::json Write(const TransformComponent& transform);
};
