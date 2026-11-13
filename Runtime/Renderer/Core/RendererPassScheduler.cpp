#include "Runtime/Renderer/Core/RendererPassScheduler.h"

#include "Runtime/Renderer/Pass/OpaqueMeshPass.h"
#include "Runtime/Renderer/Pipeline/PipelineStateCache.h"
#include "Runtime/Renderer/Pipeline/RootSignatureCache.h"
#include "Runtime/Renderer/Resource/RenderResourceServices.h"

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
