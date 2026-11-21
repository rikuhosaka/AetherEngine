#pragma once

#include "Runtime/AssetData/ModelData.h"

#include <vector>

struct PrimitiveMeshData
{
	std::vector<BasicVertex> vertices{};
	std::vector<uint32_t> indices{};
	MeshBounds bounds{};
};
