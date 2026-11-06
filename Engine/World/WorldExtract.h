#pragma once

#include "Engine/World/EntityId.h"

#include <cstdint>
#include <vector>

struct DebugStatsServices;
struct ExtractedLighting;
struct ExtractedObject;
struct ExtractedView;
struct SessionServices;
class World;

void ExtractWorld(const World& world, std::vector<ExtractedObject>& outObjects);

// View from the camera entity's world matrix and lens. Clears the output when the entity has no camera.
void ExtractCameraView(
	const World& world,
	EntityId camera,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	ExtractedView& outView);

// 描画用の抽出。Extract は補間した剛体姿勢をワールドへ書き戻してからメッシュを集める。
// 一時停止中は alpha を 1 にして、最新の固定ステップ姿勢を出す。
void Extract(
	World& world,
	std::vector<ExtractedObject>& outObjects,
	float alpha,
	const SessionServices& session,
	DebugStatsServices& stats);
// 先頭の CameraSystemComponent からビューを書く。カメラが無ければ outView を変えない。
void ExtractView(
	const World& world,
	ExtractedView& outView,
	uint32_t viewportWidth,
	uint32_t viewportHeight);
void ExtractLighting(const World& world, ExtractedLighting& outLighting);
