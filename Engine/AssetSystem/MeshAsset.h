#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <vector>

using MeshAssetId = std::string;

enum class MeshAssetKind : uint8_t
{
	TriangleList,
	ConvexHull,
	Box,
	Sphere,
	Capsule,
};

// CPU mesh source. GPU buffers stay in the renderer mesh pool.
struct MeshAsset
{
	MeshAssetId id{};
	MeshAssetKind kind = MeshAssetKind::TriangleList;
	std::vector<DirectX::XMFLOAT3> points{};
	std::vector<uint32_t> indices{};
	float width = 1.0f;
	float height = 1.0f;
	float depth = 1.0f;
	float radius = 0.5f;
	float halfHeight = 0.5f;
	uint32_t stacks = 12;
	uint32_t slices = 16;
	uint32_t hemisphereStacks = 4;
};
