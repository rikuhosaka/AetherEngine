#pragma once

#include "Runtime/Engine/AssetSystem/Model/Loader/ModelLoadTypes.h"

#include <cstdint>
#include <span>
#include <vector>

void GenerateVertexTangents(std::vector<BasicVertex>& vertices, std::span<const uint32_t> indices);
