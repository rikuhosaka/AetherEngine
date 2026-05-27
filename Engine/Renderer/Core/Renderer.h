#pragma once

#include "Engine/Renderer/Frame/FrameContext.h"
#include "Engine/Renderer/RenderItem/RenderItem.h"
#include "Engine/Renderer/Scene/RenderScene.h"

#include <memory>
#include <vector>

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

	void Initialize(
		RHIDevice* device,
		RHIDescriptorAllocator* descriptorAllocator);
	void SetFrameContext(FrameContext* frameContext);
	void BeginFrame();
	void EndFrame();

	void Submit(const RenderItem& item);
	void Render(RHICommandList* commandList);

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
	std::vector<RenderItem> m_renderItems{};
};
