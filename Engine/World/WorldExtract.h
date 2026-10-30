#pragma once

#include "Engine/World/EntityId.h"

#include <cstdint>
#include <vector>

struct DebugStatsServices;
struct EditorCameraServices;
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

void Extract(
	World& world,
	std::vector<ExtractedObject>& outObjects,
	float alpha,
	const SessionServices& session,
	DebugStatsServices& stats);
void ExtractView(
	const World& world,
	ExtractedView& outView,
	uint32_t viewportWidth,
	uint32_t viewportHeight,
	const EditorCameraServices* editorCamera);
void ExtractLighting(const World& world, ExtractedLighting& outLighting);
