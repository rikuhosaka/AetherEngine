#pragma once

#include "Engine/Renderer/Mesh/MeshTypes.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct BasicVertex
{
	float position[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float uv[2] = { 0.0f, 0.0f };
	float normal[3] = { 0.0f, 1.0f, 0.0f };
};

struct ModelSubMeshData
{
	uint32_t indexStart = 0;
	uint32_t indexCount = 0;
	uint32_t materialSlot = 0;
};

struct ModelMeshData
{
	std::string name;
	std::vector<BasicVertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<ModelSubMeshData> submeshes;
	MeshBounds bounds;
};

struct ModelMaterialSlotData
{
	std::string name;
	std::filesystem::path diffuseTexturePath;
};

struct ModelNodeData
{
	std::string name;
	float localTransform[16]{};
	int32_t parentIndex = -1;
	int32_t meshIndex = -1;
	std::vector<uint32_t> childNodeIndices;
};

struct ModelAssetData
{
	std::vector<ModelMeshData> meshes;
	std::vector<ModelMaterialSlotData> materialSlots;
	std::vector<ModelNodeData> nodes;
	uint32_t rootNodeIndex = 0;
	std::filesystem::path sourcePath;
};

struct FbxModelLoadOptions
{
	bool triangulate = true;
	bool generateNormalsIfMissing = true;
	bool convertAxisToDirectX = true;
	bool convertUnitsToMeters = true;
};
