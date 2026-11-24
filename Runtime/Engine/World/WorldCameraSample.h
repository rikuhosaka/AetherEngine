#pragma once

#include "Runtime/Engine/Camera/CameraState.h"

// ワールドから取り出したカメラ。描画用の行列はまだ作らない。
struct WorldCameraSample
{
	bool found = false;
	CameraState state{};
};
