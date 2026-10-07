#pragma once

#include <cstdint>

constexpr uint32_t kShadowMapSize = 2048;
constexpr float kShadowDepthBias = 0.002f;
// Room is 10 x 3 x 10 with the floor on y = 0. The ortho volume covers that box with margin.
constexpr float kShadowSceneCenterY = 1.5f;
constexpr float kShadowSceneRadius = 8.0f;
