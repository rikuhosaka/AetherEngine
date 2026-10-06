#pragma once

#include <vector>

struct ExtractedObject;
class World;

void ExtractWorld(const World& world, std::vector<ExtractedObject>& outObjects);
