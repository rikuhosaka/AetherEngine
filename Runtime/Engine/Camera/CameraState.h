#pragma once

#include <DirectXMath.h>

struct CameraLens
{
	float fovYDegrees = 60.0f;
	float nearPlane = 0.1f;
	float farPlane = 2000.0f;
};

// 責任: 自由飛行カメラの姿勢とレンズをまとめて保持する。
// 役割: コントローラの入出力。エンティティへ書く前の作業値としても使う。
// 所有: CameraSystemComponent のメンバ。シーン上の Transform とは別物。
struct CameraState
{
	DirectX::XMFLOAT3 position { 0.0f, 2.0f, -5.0f };
	DirectX::XMFLOAT3 forward { 0.0f, -0.371f, 0.928f };
	DirectX::XMFLOAT3 up { 0.0f, 1.0f, 0.0f };
	CameraLens lens{};
};
