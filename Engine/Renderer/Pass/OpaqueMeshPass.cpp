#include "Engine/Renderer/Pass/OpaqueMeshPass.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Pipeline/PipelineStateCache.h"
#include "Engine/Renderer/Pipeline/RootSignatureCache.h"
#include "Engine/RHI/Interface/RHICommandList.h"

namespace
{
constexpr uint32_t kTriangleListTopology = 4;
} // namespace

void OpaqueMeshPass::Execute(
	FrameContext& frameContext,
	RHICommandList* commandList,
	std::span<const RenderItem> items,
	MeshSystemServices& meshServices,
	MaterialSystemServices& materialServices,
	RootSignatureCache& rootSignatureCache,
	PipelineStateCache& pipelineStateCache)
{
	if (commandList == nullptr)
	{
		LOG_FATAL(LogCategory::Renderer, "OpaqueMeshPass::Execute called with null command list");
		return;
	}

	for (const RenderItem& item : items)
	{
		Mesh* mesh = meshServices.GetMesh(item.mesh);
		MaterialInstance* instance = materialServices.GetInstance(item.materialInstance);
		if (mesh == nullptr || instance == nullptr)
		{
			continue;
		}

		Material* material = materialServices.GetMaterial(instance->material);
		if (material == nullptr)
		{
			continue;
		}

		RHIRootSignature* rootSignature = rootSignatureCache.GetRootSignature(material->rootSignature);
		RHIPipelineState* pipelineState = pipelineStateCache.GetPipelineState(material->pipelineState);
		if (rootSignature == nullptr || pipelineState == nullptr)
		{
			continue;
		}

		commandList->SetPipelineState(pipelineState);
		commandList->SetRootSignature(rootSignature);

		if (!materialServices.GetBindCache().Bind(
				frameContext,
				commandList,
				*material,
				*instance,
				materialServices.GetTextureServices()))
		{
			LOG_ERROR(LogCategory::Renderer, "Failed to bind material for render item");
			continue;
		}

		if (mesh->vertexBuffer == nullptr || mesh->indexBuffer == nullptr)
		{
			continue;
		}

		const RHIVertexBuffer* vertexBuffers[] = { mesh->vertexBuffer.get() };
		commandList->IASetVertexBuffers(0, vertexBuffers);
		commandList->IASetIndexBuffer(mesh->indexBuffer.get());
		commandList->IASetPrimitiveTopology(kTriangleListTopology);

		if (item.submeshIndex >= mesh->submeshes.size())
		{
			continue;
		}

		const SubmeshRange& submesh = mesh->submeshes[item.submeshIndex];
		commandList->DrawIndexedInstanced(
			submesh.indexCount,
			1,
			submesh.indexStart,
			0,
			0);
	}
}
