#pragma once

#include "Engine/Renderer/Scene/RenderViewTypes.h"

#include <DirectXMath.h>

// 責任: 編集カメラの姿勢と、そこから作ったビューを公開する。
// 役割: プレイ開始時に Engine が位置と前方を読み、編集中の描画が view を使う。
// 所有: SceneLoaderSubsystem のメンバ。Context へは非所有ポインタで登録する。
struct EditorCameraServices
{
	bool valid = false;
	DirectX::XMFLOAT3 position{ 0.0f, 1.6f, -4.0f };
	DirectX::XMFLOAT3 forward{ 0.0f, 0.0f, 1.0f };
	ExtractedView view{};
};
