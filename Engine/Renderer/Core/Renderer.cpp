#include "Renderer.h"

#include "Engine/Renderer/Pass/OpaqueMeshPass.h"
#include "Engine/Renderer/Pipeline/PipelineStateCache.h"
#include "Engine/Renderer/Pipeline/RootSignatureCache.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/ShaderSystem/ShaderSystemServices.h"
#include "Engine/RHI/Interface/RHITransientDescriptorAllocator.h"
#include "Engine/RHI/Interface/RHIUploadBuffer.h"

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

void Renderer::Initialize(RHIDevice* device, RHIDescriptorAllocator* descriptorAllocator)
{
	m_shaderServices = ShaderSystemServices::Create();
	m_rootSignatureCache = std::make_unique<RootSignatureCache>(device);
	m_pipelineStateCache = std::make_unique<PipelineStateCache>(device, m_rootSignatureCache.get());

	std::string error;
	m_resourceServices = RenderResourceServices::Create(
		device,
		descriptorAllocator,
		m_shaderServices.get(),
		m_rootSignatureCache.get(),
		m_pipelineStateCache.get(),
		&error);
}

void Renderer::SetFrameContext(FrameContext* frameContext)
{
	m_frameContext = frameContext;
}

void Renderer::BeginFrame()
{
	m_renderItems.clear();
	m_scene.BeginFrame();
	if (m_frameContext != nullptr && m_frameContext->transientDescriptors != nullptr)
	{
		m_frameContext->transientDescriptors->Reset();
	}
	if (m_frameContext != nullptr && m_frameContext->uploadBuffer != nullptr)
	{
		m_frameContext->uploadBuffer->Reset();
	}
}

void Renderer::EndFrame()
{
}

void Renderer::Submit(const RenderItem& item)
{
	m_renderItems.push_back(item);
}

void Renderer::Render(RHICommandList* commandList)
{
	if (m_frameContext == nullptr || m_resourceServices == nullptr || commandList == nullptr)
	{
		return;
	}

	OpaqueMeshPass::Execute(
		*m_frameContext,
		commandList,
		m_renderItems,
		m_resourceServices->GetMeshServices(),
		m_resourceServices->GetMaterialServices(),
		*m_rootSignatureCache,
		*m_pipelineStateCache);
}
