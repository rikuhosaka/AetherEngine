#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/SceneDocument/SceneComponents.h"
#include "ThirdParty/nlohmann/json.hpp"

#include <string_view>

// 責任: 文書上の剛体と physics オブジェクトの変換を持つ。
// 役割: 形状と運動を読み書きする。親やモデルとの排他は呼び出し側が検査する。
// 所有: 状態を持たない。
class RigidBodySerializer
{
public:
	static constexpr std::string_view kKey = "physics";

	[[nodiscard]] static Result<SceneRigidBody> Read(const nlohmann::json& value);
	[[nodiscard]] static nlohmann::json Write(const SceneRigidBody& body);
	[[nodiscard]] static const char* MotionName(PhysicsMotion motion);
	[[nodiscard]] static const char* ShapeName(PhysicsShapeType type);
};
