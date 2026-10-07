#pragma once

#include "Engine/Frame/FrameContext.h"
#include "Engine/Renderer/Core/ShaderSourcePolicy.h"

#include <filesystem>
#include <memory>

class PipelineStateCache;
class RHICommandList;
class RHIDevice;
class RHIPixelShader;
class RHIPipelineState;
class RHIRootSignature;
class RHIVertexShader;
class RootSignatureCache;
class ShaderSystemServices;

class SceneDepthDebugPass
{
public:
	void Configure(
		RHIDevice* device,
		ShaderSystemServices* shaderServices,
		RootSignatureCache* rootSignatureCache,
		PipelineStateCache* pipelineStateCache,
		const std::filesystem::path& shaderRoot,
		const std::filesystem::path& compiledShaderRoot,
		ShaderSourcePolicy shaderSourcePolicy);

	void Execute(FrameContext& frameContext, RHICommandList* commandList);

private:
	[[nodiscard]] bool EnsureReady();

	RHIDevice* m_device = nullptr;
	ShaderSystemServices* m_shaderServices = nullptr;
	RootSignatureCache* m_rootSignatureCache = nullptr;
	PipelineStateCache* m_pipelineStateCache = nullptr;
	std::filesystem::path m_shaderRoot{};
	std::filesystem::path m_compiledShaderRoot{};
	ShaderSourcePolicy m_shaderSourcePolicy = ShaderSourcePolicy::PreferPrecompiled;

	std::unique_ptr<RHIVertexShader> m_vertexShader{};
	std::unique_ptr<RHIPixelShader> m_pixelShader{};
	RHIRootSignature* m_rootSignature = nullptr;
	RHIPipelineState* m_pipelineState = nullptr;
	uint32_t m_srvRootParameter = 0;
	bool m_ready = false;
	bool m_failed = false;
};
