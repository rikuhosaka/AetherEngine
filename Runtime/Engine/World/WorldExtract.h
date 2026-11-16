#pragma once

#include "Runtime/Engine/World/EntityId.h"
#include "Runtime/Engine/World/ExtractedObject.h"
#include "Runtime/Engine/World/RenderPose.h"

#include <cstdint>
#include <span>
#include <vector>

struct DebugStatsServices;
struct ExtractedLighting;
struct ExtractedView;
class World;

void ExtractWorld(
	const World& world,
	std::vector<WorldExtractedObject>& outObjects,
	std::span<const RenderPose> poses = {});

// View from the camera entity's world matrix and lens. Clears the output when the entity has no camera.
void ExtractCameraView(
	const World& world,
	EntityId camera,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	ExtractedView& outView,
	std::span<const RenderPose> poses = {});

// プレイ中の PhysicsSubsystem から、alpha の補間姿勢を集める。無ければ out を空にする。
// alpha 0 はステップ前、1 はステップ後。Transform は変えない。
void SampleRenderPoses(const World& world, float alpha, std::vector<RenderPose>& out);

// 描画用の抽出。poses の位置と回転でワールド行列を作る。Transform は変えない。
void Extract(
	const World& world,
	std::vector<WorldExtractedObject>& outObjects,
	std::span<const RenderPose> poses,
	DebugStatsServices& stats);
// 先頭の CameraSystemComponent からビューを書く。カメラが無ければ outView を変えない。
void ExtractView(
	const World& world,
	ExtractedView& outView,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	std::span<const RenderPose> poses = {});
void ExtractLighting(const World& world, ExtractedLighting& outLighting);
