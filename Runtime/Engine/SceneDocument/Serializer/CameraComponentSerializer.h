#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/World/Component/CameraComponent.h"
#include "ThirdParty/nlohmann/json.hpp"

#include <string_view>

// 責任: 文書上のカメラと camera オブジェクトの変換を持つ。
// 役割: 視野角とクリップ距離を読み書きする。
// 所有: 状態を持たない。
class CameraComponentSerializer
{
public:
	static constexpr std::string_view kKey = "camera";

	[[nodiscard]] static Result<CameraComponent> Read(const nlohmann::json& value);
	[[nodiscard]] static nlohmann::json Write(const CameraComponent& camera);
};
