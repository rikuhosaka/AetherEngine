#pragma once

struct ExtractedLighting;
class World;

void BuildExtractedLighting(const World& world, ExtractedLighting& outLighting);
