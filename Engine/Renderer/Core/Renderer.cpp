#include "Renderer.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Core/RendererPassScheduler.h"
#include "Engine/Renderer/Pipeline/PipelineStateCache.h"
#include "Engine/Renderer/Pipeline/RootSignatureCache.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/ShaderSystem/ShaderSystemServices.h"
#include "Engine/RHI/Interface/RHICommandList.h"

#include <cassert>

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

Result<void> Renderer::Initialize(
	RHIDevice* device,
	RHIDescriptorAllocator* descriptorAllocator,
	const RendererConfig& config)
{
	if (device == nullptr || descriptorAllocator == nullptr)
	{
		return FailInternal(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"Renderer requires a valid device and descriptor allocator");
	}

	auto shaderServicesResult = ShaderSystemServices::Create();
	if (!shaderServicesResult)
	{
		LogResult(shaderServicesResult, LogCategory::Renderer);
		return MakeFail(shaderServicesResult.error.code, shaderServicesResult.error.message);
	}
	m_shaderServices = std::move(shaderServicesResult.value);

	m_rootSignatureCache = std::make_unique<RootSignatureCache>(device);
	m_pipelineStateCache = std::make_unique<PipelineStateCache>(device, m_rootSignatureCache.get());

	auto resourceServicesResult = RenderResourceServices::Create(
		device,
		descriptorAllocator,
		m_shaderServices.get(),
		m_rootSignatureCache.get(),
		m_pipelineStateCache.get(),
		config);
	if (!resourceServicesResult)
	{
		LogResult(resourceServicesResult, LogCategory::Renderer);
		return MakeFail(
			resourceServicesResult.error.code,
			resourceServicesResult.error.message);
	}
	m_resourceServices = std::move(resourceServicesResult.value);

	m_shaderRoot = config.shaderRoot;
	m_visualizeSceneDepth = config.visualizeSceneDepth;
	m_sceneDepthDebug.Configure(
		device,
		m_shaderServices.get(),
		m_rootSignatureCache.get(),
		m_pipelineStateCache.get(),
		config.shaderRoot,
		config.compiledShaderRoot,
		config.shaderSourcePolicy);
	if (auto shadowResult = m_shadowDepth.Initialize(
			device,
			m_shaderServices.get(),
			m_rootSignatureCache.get(),
			m_pipelineStateCache.get(),
			config.shaderRoot,
			config.compiledShaderRoot,
			config.shaderSourcePolicy);
		!shadowResult)
	{
		return shadowResult;
	}
	m_scene.SetShaderRoot(m_shaderRoot);
	return MakeOk();
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
}

void Renderer::ExtractScene(std::span<const ExtractedObject> objects)
{
	m_scene.Extract(objects, m_frameIndex);
}

void Renderer::ExtractView(const ExtractedView& view)
{
	m_scene.ExtractView(view);
}

void Renderer::ExtractLighting(const ExtractedLighting& lighting)
{
	m_scene.ExtractLighting(lighting);
}

void Renderer::BuildScene(RHICommandList* commandList)
{
	if (m_frameContext == nullptr || m_resourceServices == nullptr || commandList == nullptr)
	{
		LOG_FATAL(LogCategory::Renderer, "Renderer::BuildScene called with invalid state");
		return;
	}
	if (m_shaderRoot.empty())
	{
		LOG_FATAL(LogCategory::Renderer, "Renderer::BuildScene called without shader root");
		return;
	}

	m_scene.Build(*m_frameContext, commandList, *m_resourceServices);
	m_sceneBuilt = true;
}

void Renderer::EndFrame()
{
}

void Renderer::RenderShadow(RHICommandList* commandList)
{
	if (m_frameContext == nullptr || m_resourceServices == nullptr || commandList == nullptr || !m_sceneBuilt)
	{
		LOG_FATAL(LogCategory::Renderer, "Renderer::RenderShadow called with invalid state");
		return;
	}

	m_shadowDepth.Execute(
		*m_frameContext,
		commandList,
		m_scene.GetSnapshot(),
		m_resourceServices->GetMeshServices());
}

void Renderer::Render(RHICommandList* commandList)
{
	if (m_frameContext == nullptr || m_resourceServices == nullptr || commandList == nullptr || !m_sceneBuilt)
	{
		LOG_FATAL(LogCategory::Renderer, "Renderer::Render called with invalid state");
		return;
	}

	RendererPassScheduler::ExecuteAll(
		*m_frameContext,
		commandList,
		m_scene.GetSnapshot(),
		*m_resourceServices,
		*m_rootSignatureCache,
		*m_pipelineStateCache,
		m_shadowDepth.GetTexture());
}

void Renderer::DrawSceneDepthDebug(RHICommandList* commandList)
{
	if (!m_visualizeSceneDepth || m_frameContext == nullptr || commandList == nullptr)
	{
		return;
	}

	m_sceneDepthDebug.Execute(*m_frameContext, commandList);
}
