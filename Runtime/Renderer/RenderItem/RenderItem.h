#pragma once

#include "Runtime/Renderer/Material/MaterialTypes.h"
#include "Runtime/Renderer/Mesh/MeshTypes.h"
#include "Runtime/Renderer/Scene/RenderObjectId.h"

struct RenderItem
{
	RenderObjectId objectId{};
	MeshHandle mesh{};
	MaterialInstanceHandle materialInstance{};
	uint32_t submeshIndex = 0;
	uint32_t sortKey = 0;
	uint32_t objectConstantsIndex = UINT32_MAX;
};
