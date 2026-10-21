#pragma once

#include "Engine/AssetSystem/MeshBounds.h"
#include "Engine/Core/Handle/Handle.h"
#include "Engine/RHI/Interface/RHIBuffer.h"

#include <cstdint>
#include <memory>
#include <vector>

enum class VertexLayoutId : uint32_t
{
	PositionTex = 0,
	Basic = 1,
};

struct SubmeshRange
{
	uint32_t indexStart = 0;
	uint32_t indexCount = 0;
	uint32_t materialSlot = 0;
};

struct Mesh
{
	std::unique_ptr<RHIVertexBuffer> vertexBuffer{};
	std::unique_ptr<RHIIndexBuffer> indexBuffer{};
	VertexLayoutId layoutId = VertexLayoutId::PositionTex;
	std::vector<SubmeshRange> submeshes{};
	MeshBounds bounds{};
};

using MeshHandle = Handle<Mesh>;
