#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/Resource/FrameContext.h"
#include "Runtime/Renderer/Core/ShaderSourcePolicy.h"
#include "Runtime/Renderer/Material/MaterialTypes.h"
#include "Runtime/Renderer/Scene/RenderSceneTypes.h"
#include "Runtime/RHI/Common/RHIDescriptor.h"
#include "Runtime/RHI/Interface/RHIDSVAllocator.h"
#include "Runtime/RHI/Interface/RHITexture.h"

#include <filesystem>
#include <memory>

class MeshSystemServices;
class PipelineStateCache;
class RHICommandList;
class RHIDevice;
class RHIPipelineState;
class RHIRootSignature;
class RootSignatureCache;
class ShaderSystemServices;

class ShadowDepthPass
{
public:
	~ShadowDepthPass();

	Result<void> Initialize(
		RHIDevice* device,
		ShaderSystemServices* shaderServices,
		RootSignatureCache* rootSignatureCache,
		PipelineStateCache* pipelineStateCache,
		const std::filesystem::path& shaderRoot,
		const std::filesystem::path& compiledShaderRoot,
		ShaderSourcePolicy shaderSourcePolicy);

	void Execute(
		FrameContext& frameContext,
		RHICommandList* commandList,
		const RenderFrameSnapshot& snapshot,
		MeshSystemServices& meshServices);

	[[nodiscard]] RHITexture* GetTexture() const noexcept { return m_texture.get(); }

private:
	[[nodiscard]] bool EnsurePipeline();

	RHIDevice* m_device = nullptr;
	ShaderSystemServices* m_shaderServices = nullptr;
	RootSignatureCache* m_rootSignatureCache = nullptr;
	PipelineStateCache* m_pipelineStateCache = nullptr;
	std::filesystem::path m_shaderRoot{};
	std::filesystem::path m_compiledShaderRoot{};
	ShaderSourcePolicy m_shaderSourcePolicy = ShaderSourcePolicy::PreferPrecompiled;

	std::unique_ptr<RHIDSVAllocator> m_dsvAllocator{};
	std::unique_ptr<RHITexture> m_texture{};
	DsvHandle m_dsv{};
	bool m_shaderReadable = false;

	Material m_material{};
	RHIRootSignature* m_rootSignature = nullptr;
	RHIPipelineState* m_pipelineState = nullptr;
	bool m_pipelineReady = false;
	bool m_pipelineFailed = false;
};
