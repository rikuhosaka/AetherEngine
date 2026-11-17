#pragma once

#include "Runtime/Engine/World/Component/Component.h"

#include <DirectXMath.h>

// 責任: エンティティのローカル位置、回転、拡縮を保持する。
// 役割: 親子のワールド行列を作る。文書の transform オブジェクトはこの値を読む。
// 所有: 付いている Actor がコンポーネントとして持つ。Spawn が必ず1つ足す。
struct TransformComponent : Component
{
	DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
};
