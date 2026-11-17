#pragma once

#include "Runtime/Engine/World/EntityId.h"

#include <DirectXMath.h>

// 描画フレーム限りの剛体姿勢。位置と回転だけを持つ。スケールは TransformComponent のまま。
struct RenderPose
{
	EntityId entity{};
	DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
};
