#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Core/RendererConfig.h"
#include "Engine/Frame/FrameContext.h"
#include "Engine/Renderer/Pass/SceneDepthDebugPass.h"
#include "Engine/Renderer/Pass/ShadowDepthPass.h"
#include "Engine/Renderer/Scene/RenderScene.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

class PipelineStateCache;
class RHIDescriptorAllocator;
class RHIDevice;
class RHICommandList;
class RenderResourceServices;
class RootSignatureCache;
class ShaderSystemServices;

class Renderer
{
public:
	Renderer();
	~Renderer();

	Result<void> Initialize(
		RHIDevice* device,
		RHIDescriptorAllocator* descriptorAllocator,
		const RendererConfig& config);
	void SetFrameContext(FrameContext* frameContext);

	void BeginFrame(uint32_t frameIndex);
	void ExtractScene(std::span<const ExtractedObject> objects);
	void ExtractView(const ExtractedView& view);
	void ExtractLighting(const ExtractedLighting& lighting);
	void BuildScene(RHICommandList* commandList);
	void RenderShadow(RHICommandList* commandList);
	void Render(RHICommandList* commandList);
	void DrawSceneDepthDebug(RHICommandList* commandList);
	void EndFrame();

	[[nodiscard]] bool IsSceneBuilt() const noexcept { return m_sceneBuilt; }

	[[nodiscard]] RenderScene& GetScene() noexcept { return m_scene; }
	[[nodiscard]] const RenderScene& GetScene() const noexcept { return m_scene; }

	[[nodiscard]] RenderResourceServices* GetResourceServices() noexcept { return m_resourceServices.get(); }
	[[nodiscard]] ShaderSystemServices* GetShaderServices() noexcept { return m_shaderServices.get(); }
	[[nodiscard]] RootSignatureCache* GetRootSignatureCache() noexcept { return m_rootSignatureCache.get(); }
	[[nodiscard]] PipelineStateCache* GetPipelineStateCache() noexcept { return m_pipelineStateCache.get(); }

private:
	std::unique_ptr<ShaderSystemServices> m_shaderServices{};
	std::unique_ptr<RootSignatureCache> m_rootSignatureCache{};
	std::unique_ptr<PipelineStateCache> m_pipelineStateCache{};
	std::unique_ptr<RenderResourceServices> m_resourceServices{};

	FrameContext* m_frameContext = nullptr;
	RenderScene m_scene{};

	std::filesystem::path m_shaderRoot{};
	SceneDepthDebugPass m_sceneDepthDebug{};
	ShadowDepthPass m_shadowDepth{};
	uint32_t m_frameIndex = 0;
	bool m_sceneBuilt = false;
	bool m_visualizeSceneDepth = false;
};
