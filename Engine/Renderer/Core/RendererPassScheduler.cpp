#include "Engine/Renderer/Core/RendererPassScheduler.h"

#include "Engine/Renderer/Pass/OpaqueMeshPass.h"
#include "Engine/Renderer/Pipeline/PipelineStateCache.h"
#include "Engine/Renderer/Pipeline/RootSignatureCache.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"

void RendererPassScheduler::ExecuteAll(
	FrameContext& frameContext,
	RHICommandList* commandList,
	const RenderFrameSnapshot& snapshot,
	RenderResourceServices& resources,
	RootSignatureCache& rootSignatureCache,
	PipelineStateCache& pipelineStateCache,
	RHITexture* shadowMap)
{
	// Fixed pass order. Register new passes here as they are implemented.
	OpaqueMeshPass::Execute(
		frameContext,
		commandList,
		snapshot,
		snapshot.opaqueItems,
		resources.GetMeshServices(),
		resources.GetMaterialServices(),
		rootSignatureCache,
		pipelineStateCache,
		shadowMap);
	OpaqueMeshPass::Execute(
		frameContext,
		commandList,
		snapshot,
		snapshot.transparentItems,
		resources.GetMeshServices(),
		resources.GetMaterialServices(),
		rootSignatureCache,
		pipelineStateCache,
		shadowMap);
}
