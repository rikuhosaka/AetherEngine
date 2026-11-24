#pragma once

#include "Runtime/Engine/World/RenderPose.h"
#include "Runtime/Engine/World/WorldCameraSample.h"
#include "Runtime/Renderer/Scene/RenderSceneTypes.h"

#include <cstdint>
#include <span>

struct DebugStatsServices;
class World;

// ワールドのカメラ、ライト、Renderable から、そのフレームの描画入力を書く。
void BuildRenderFrame(
	const World& world,
	const WorldCameraSample& camera,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	std::span<const RenderPose> poses,
	DebugStatsServices& stats,
	RenderFrame& out);
