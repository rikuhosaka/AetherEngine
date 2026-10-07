#pragma once

#include "Engine/Frame/FrameContext.h"
#include "Engine/Renderer/RenderItem/RenderItem.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"

#include <span>

class MaterialSystemServices;
class MeshSystemServices;
class PipelineStateCache;
class RootSignatureCache;
class RHICommandList;
class RHITexture;

class OpaqueMeshPass
{
public:
	static void Execute(
		FrameContext& frameContext,
		RHICommandList* commandList,
		const RenderFrameSnapshot& snapshot,
		std::span<const RenderItem> items,
		MeshSystemServices& meshServices,
		MaterialSystemServices& materialServices,
		RootSignatureCache& rootSignatureCache,
		PipelineStateCache& pipelineStateCache,
		RHITexture* shadowMap);
};
