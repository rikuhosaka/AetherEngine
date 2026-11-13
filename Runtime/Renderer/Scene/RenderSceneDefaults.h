#pragma once

#include <cstdint>

struct ExtractedLighting;
struct ExtractedView;

void InitDefaultExtractedView(ExtractedView& outView, uint32_t viewportWidth, uint32_t viewportHeight);

void InitDefaultExtractedLighting(ExtractedLighting& outLighting);
