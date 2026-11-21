#pragma once

struct WorldExtractedLighting;
class World;

void BuildExtractedLighting(const World& world, WorldExtractedLighting& outLighting);
