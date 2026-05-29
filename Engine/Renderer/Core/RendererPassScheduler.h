#pragma once

#include "Engine/Frame/FrameContext.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"

class PipelineStateCache;
class RenderResourceServices;
class RootSignatureCache;
class RHICommandList;

class RendererPassScheduler
{
public:
	static void ExecuteAll(
		FrameContext& frameContext,
		RHICommandList* commandList,
		const RenderFrameSnapshot& snapshot,
		RenderResourceServices& resources,
		RootSignatureCache& rootSignatureCache,
		PipelineStateCache& pipelineStateCache);
};
