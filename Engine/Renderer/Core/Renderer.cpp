#include "Renderer.h"

#include "Engine/Renderer/Core/RendererPassScheduler.h"
#include "Engine/Renderer/Pipeline/PipelineStateCache.h"
#include "Engine/Renderer/Pipeline/RootSignatureCache.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/ShaderSystem/ShaderSystemServices.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Interface/RHITransientDescriptorAllocator.h"
#include "Engine/RHI/Interface/RHIUploadBuffer.h"

#include <cassert>

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

void Renderer::Initialize(
	RHIDevice* device,
	RHIDescriptorAllocator* descriptorAllocator,
	const RendererConfig& config)
{
	assert(device != nullptr);
	assert(descriptorAllocator != nullptr);

	m_shaderServices = ShaderSystemServices::Create();
	assert(m_shaderServices != nullptr);

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
	assert(m_resourceServices != nullptr);

	m_shaderRoot = config.shaderRoot;
	m_scene.SetShaderRoot(m_shaderRoot);
}

void Renderer::SetFrameContext(FrameContext* frameContext)
{
	m_frameContext = frameContext;
}

void Renderer::BeginFrame(uint32_t frameIndex)
{
	m_frameIndex = frameIndex;
	m_sceneBuilt = false;
	m_scene.BeginFrame();
	if (m_frameContext != nullptr && m_frameContext->graphicsCommandList != nullptr)
	{
		m_frameContext->graphicsCommandList->Reset();
	}
	if (m_frameContext != nullptr && m_frameContext->transientDescriptors != nullptr)
	{
		m_frameContext->transientDescriptors->Reset();
	}
	if (m_frameContext != nullptr && m_frameContext->uploadBuffer != nullptr)
	{
		m_frameContext->uploadBuffer->Reset();
	}
}

void Renderer::ExtractScene(std::span<const ExtractedObject> objects)
{
	m_scene.Extract(objects, m_frameIndex);
}

void Renderer::BuildScene(RHICommandList* commandList)
{
	assert(m_frameContext != nullptr);
	assert(m_resourceServices != nullptr);
	assert(commandList != nullptr);
	assert(!m_shaderRoot.empty());

	m_scene.Build(*m_frameContext, commandList, *m_resourceServices);
	m_sceneBuilt = true;
}

void Renderer::EndFrame()
{
}

void Renderer::Render(RHICommandList* commandList)
{
	assert(m_frameContext != nullptr);
	assert(m_resourceServices != nullptr);
	assert(commandList != nullptr);
	assert(m_sceneBuilt);

	RendererPassScheduler::ExecuteAll(
		*m_frameContext,
		commandList,
		m_scene.GetSnapshot(),
		*m_resourceServices,
		*m_rootSignatureCache,
		*m_pipelineStateCache);
}
