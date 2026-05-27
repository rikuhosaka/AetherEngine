#pragma once

#include "Engine/Renderer/Frame/FrameContext.h"
#include "Engine/Renderer/RenderItem/RenderItem.h"

#include <span>

class MaterialSystemServices;
class MeshSystemServices;
class PipelineStateCache;
class RootSignatureCache;
class RHICommandList;

class OpaqueMeshPass
{
public:
	static void Execute(
		FrameContext& frameContext,
		RHICommandList* commandList,
		std::span<const RenderItem> items,
		MeshSystemServices& meshServices,
		MaterialSystemServices& materialServices,
		RootSignatureCache& rootSignatureCache,
		PipelineStateCache& pipelineStateCache);
};
