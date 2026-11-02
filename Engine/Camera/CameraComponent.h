#pragma once

#include "Engine/World/Component.h"

// 責任: エンティティカメラのレンズ設定を保持する。
// 役割: 視野角とクリップ距離を描画抽出へ渡す。位置と向きは同じエンティティの Transform が持つ。
// 所有: 付いている Actor がコンポーネントとして持つ。CameraSubsystem はこれを所有しない。
struct CameraComponent : Component
{
	float fovYDegrees = 60.0f;
	float nearPlane = 0.1f;
	float farPlane = 2000.0f;
};
