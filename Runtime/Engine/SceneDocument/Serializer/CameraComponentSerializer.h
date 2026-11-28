#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/SceneDocument/SceneComponents.h"
#include "ThirdParty/nlohmann/json.hpp"

#include <string_view>

// 責任: 文書上のカメラと camera オブジェクトの変換を持つ。
// 役割: 視野角とクリップ距離を読み書きする。
// 所有: 状態を持たない。
class CameraComponentSerializer
{
public:
	static constexpr std::string_view kKey = "camera";

	[[nodiscard]] static Result<SceneCamera> Read(const nlohmann::json& value);
	[[nodiscard]] static nlohmann::json Write(const SceneCamera& camera);
};
