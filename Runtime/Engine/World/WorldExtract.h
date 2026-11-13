#pragma once

#include "Runtime/Engine/World/EntityId.h"
#include "Runtime/Engine/World/ExtractedObject.h"

#include <cstdint>
#include <vector>

struct DebugStatsServices;
struct ExtractedLighting;
struct ExtractedView;
class World;

void ExtractWorld(const World& world, std::vector<WorldExtractedObject>& outObjects);

// View from the camera entity's world matrix and lens. Clears the output when the entity has no camera.
void ExtractCameraView(
	const World& world,
	EntityId camera,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	ExtractedView& outView);

// 描画用の抽出。Extract は補間した剛体姿勢をワールドへ書き戻してからメッシュを集める。
// 一時停止中の alpha は呼び出し側が 1 にする。
void Extract(
	World& world,
	std::vector<WorldExtractedObject>& outObjects,
	float alpha,
	DebugStatsServices& stats);
// 先頭の CameraSystemComponent からビューを書く。カメラが無ければ outView を変えない。
void ExtractView(
	const World& world,
	ExtractedView& outView,
	uint32_t viewportWidth,
	uint32_t viewportHeight);
void ExtractLighting(const World& world, ExtractedLighting& outLighting);
