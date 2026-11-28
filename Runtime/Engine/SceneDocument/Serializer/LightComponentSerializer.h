#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/SceneDocument/SceneComponents.h"
#include "ThirdParty/nlohmann/json.hpp"

#include <string_view>

// 責任: 文書上のライトと light オブジェクトの変換を持つ。
// 役割: 色と強度を読み書きする。
// 所有: 状態を持たない。
class LightComponentSerializer
{
public:
	static constexpr std::string_view kKey = "light";

	[[nodiscard]] static Result<SceneLight> Read(const nlohmann::json& value);
	[[nodiscard]] static nlohmann::json Write(const SceneLight& light);
};
