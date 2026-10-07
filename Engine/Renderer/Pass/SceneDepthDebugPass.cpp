#include "Engine/Renderer/Pass/SceneDepthDebugPass.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Pipeline/PipelineStateCache.h"
#include "Engine/Renderer/Pipeline/RootSignatureCache.h"
#include "Engine/Renderer/Pipeline/ShaderRootLayoutBuilder.h"
#include "Engine/Renderer/ShaderSystem/Cache/ShaderBytecodeCache.h"
#include "Engine/Renderer/ShaderSystem/Cache/ShaderReflectionCache.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"
#include "Engine/Renderer/ShaderSystem/ShaderSystemServices.h"
#include "Engine/RHI/Common/RHIPipelineStateLayout.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Common/RHIScopedDebugEvent.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHIShader.h"
#include "Engine/RHI/Interface/RHITransientDescriptorAllocator.h"

#include <algorithm>
#include <cctype>

namespace
{
constexpr uint32_t kTriangleListTopology = 4;

struct ResolvedShaderAsset
{
	std::filesystem::path path{};
	bool isPrecompiled = false;
};

bool IsPrecompiledShaderExtension(const std::filesystem::path& path)
{
	std::string extension = path.extension().string();
	std::ranges::transform(extension, extension.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return extension == ".cso";
}

bool PathExists(const std::filesystem::path& path)
{
	std::error_code errorCode{};
	return std::filesystem::exists(path, errorCode);
}

ResolvedShaderAsset ResolveShaderAssetPath(
	const std::filesystem::path& sourcePath,
	const std::filesystem::path& compiledShaderRoot,
	ShaderSourcePolicy policy)
{
	if (IsPrecompiledShaderExtension(sourcePath))
	{
		return { sourcePath, true };
	}

	const std::filesystem::path csoPath = compiledShaderRoot.empty()
		? std::filesystem::path{}
		: compiledShaderRoot / (sourcePath.stem().string() + ".cso");
	const bool csoExists = !csoPath.empty() && PathExists(csoPath);
	const bool hlslExists = PathExists(sourcePath);

	switch (policy)
	{
	case ShaderSourcePolicy::PrecompiledOnly:
		return { csoPath, true };

	case ShaderSourcePolicy::SourceOnly:
		return { sourcePath, false };

	case ShaderSourcePolicy::PreferPrecompiled:
		if (csoExists)
		{
			return { csoPath, true };
		}
		return { sourcePath, false };

	case ShaderSourcePolicy::PreferSource:
		if (hlslExists)
		{
			return { sourcePath, false };
		}
		if (csoExists)
		{
			return { csoPath, true };
		}
		return { sourcePath, false };
	}

	return { sourcePath, false };
}
} // namespace

void SceneDepthDebugPass::Configure(
	RHIDevice* device,
	ShaderSystemServices* shaderServices,
	RootSignatureCache* rootSignatureCache,
	PipelineStateCache* pipelineStateCache,
	const std::filesystem::path& shaderRoot,
	const std::filesystem::path& compiledShaderRoot,
	ShaderSourcePolicy shaderSourcePolicy)
{
	m_device = device;
	m_shaderServices = shaderServices;
	m_rootSignatureCache = rootSignatureCache;
	m_pipelineStateCache = pipelineStateCache;
	m_shaderRoot = shaderRoot;
	m_compiledShaderRoot = compiledShaderRoot;
	m_shaderSourcePolicy = shaderSourcePolicy;
}

void SceneDepthDebugPass::Execute(FrameContext& frameContext, RHICommandList* commandList)
{
	if (commandList == nullptr || frameContext.depthTexture == nullptr)
	{
		return;
	}

	if (!EnsureReady())
	{
		return;
	}

	if (frameContext.transientDescriptors == nullptr)
	{
		LOG_ERROR(LogCategory::Renderer, "Scene depth debug pass requires transient descriptors");
		return;
	}

	const RHIScopedDebugEvent passEvent(commandList, "SceneDepthDebug");
	commandList->SetTransientDescriptorHeap(frameContext.transientDescriptors);

	const uint32_t descriptorIndex = frameContext.transientDescriptors->Allocate();
	if (descriptorIndex == UINT32_MAX)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to allocate a scene depth shader resource view");
		return;
	}

	const CpuDescHandle cpuHandle = frameContext.transientDescriptors->GetCpuHandle(descriptorIndex);
	const GpuDescHandle gpuHandle = frameContext.transientDescriptors->GetGpuHandle(descriptorIndex);
	m_device->WriteShaderResourceView(frameContext.depthTexture, cpuHandle);

	commandList->SetPipelineState(m_pipelineState);
	commandList->SetRootSignature(m_rootSignature);
	commandList->IASetPrimitiveTopology(kTriangleListTopology);
	commandList->SetGraphicsRootDescriptorTable(m_srvRootParameter, gpuHandle.ptr);
	commandList->DrawInstanced(3, 1, 0, 0);
}

bool SceneDepthDebugPass::EnsureReady()
{
	if (m_ready)
	{
		return true;
	}
	if (m_failed)
	{
		return false;
	}

	if (m_device == nullptr || m_shaderServices == nullptr || !m_shaderServices->IsInitialized()
		|| m_rootSignatureCache == nullptr || m_pipelineStateCache == nullptr || m_shaderRoot.empty())
	{
		LOG_ERROR(LogCategory::Renderer, "Scene depth debug pass is not configured");
		m_failed = true;
		return false;
	}

	const auto acquireBytecode = [this](const std::filesystem::path& sourcePath, ShaderStage stage)
		-> Result<ShaderBytecodeHandle>
	{
		const ResolvedShaderAsset asset = ResolveShaderAssetPath(
			sourcePath,
			m_compiledShaderRoot,
			m_shaderSourcePolicy);

		ShaderCompileDesc compileDesc{};
		compileDesc.FilePath = asset.path;
		compileDesc.Stage = stage;
		compileDesc.EntryPoint = sourcePath.stem().string();
		if (!asset.isPrecompiled)
		{
			compileDesc.IncludeDirectories.push_back(sourcePath.parent_path());
		}

		return m_shaderServices->GetBytecodeCache().Acquire(compileDesc);
	};

	const std::filesystem::path vertexPath = m_shaderRoot / "SceneDepthVS.hlsl";
	const std::filesystem::path pixelPath = m_shaderRoot / "SceneDepthPS.hlsl";

	auto vsBytecodeResult = acquireBytecode(vertexPath, ShaderStage::Vertex);
	auto psBytecodeResult = acquireBytecode(pixelPath, ShaderStage::Pixel);
	if (!vsBytecodeResult || !psBytecodeResult)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to load scene depth debug shaders");
		m_failed = true;
		return false;
	}

	ShaderBytecodeCache& bytecodeCache = m_shaderServices->GetBytecodeCache();
	ShaderReflectionCache& reflectionCache = m_shaderServices->GetReflectionCache();
	const ShaderBytecode* vsBytecode = bytecodeCache.GetBytecode(vsBytecodeResult.value);
	const ShaderBytecode* psBytecode = bytecodeCache.GetBytecode(psBytecodeResult.value);
	if (vsBytecode == nullptr || psBytecode == nullptr)
	{
		LOG_ERROR(LogCategory::Renderer, "Scene depth debug shader bytecode is missing");
		m_failed = true;
		return false;
	}

	auto vsReflectionResult = reflectionCache.GetOrReflect(
		vsBytecodeResult.value,
		bytecodeCache,
		ShaderStage::Vertex);
	auto psReflectionResult = reflectionCache.GetOrReflect(
		psBytecodeResult.value,
		bytecodeCache,
		ShaderStage::Pixel);
	if (!vsReflectionResult || !psReflectionResult)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to reflect scene depth debug shaders");
		m_failed = true;
		return false;
	}

	const ShaderReflectionData* vsReflection = reflectionCache.GetData(vsReflectionResult.value);
	const ShaderReflectionData* psReflection = reflectionCache.GetData(psReflectionResult.value);
	if (vsReflection == nullptr || psReflection == nullptr)
	{
		LOG_ERROR(LogCategory::Renderer, "Scene depth debug shader reflection is missing");
		m_failed = true;
		return false;
	}

	ShaderRootLayoutBuilder layoutBuilder{};
	layoutBuilder.AddStage(*vsReflection);
	layoutBuilder.AddStage(*psReflection);
	auto layoutResult = layoutBuilder.Build();
	if (!layoutResult)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to build the scene depth debug root signature");
		m_failed = true;
		return false;
	}

	bool foundSrv = false;
	for (const ShaderRootBindingSlot& slot : layoutResult.value.Slots)
	{
		if (slot.RootType == RHIRootParamType::SRV)
		{
			m_srvRootParameter = slot.RootParameterIndex;
			foundSrv = true;
			break;
		}
	}
	if (!foundSrv)
	{
		LOG_ERROR(LogCategory::Renderer, "Scene depth debug shader is missing a texture binding");
		m_failed = true;
		return false;
	}

	auto rootSignatureResult = m_rootSignatureCache->GetOrCreateRootSignature(layoutResult.value.Layout);
	if (!rootSignatureResult)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to create the scene depth debug root signature");
		m_failed = true;
		return false;
	}
	m_rootSignature = m_rootSignatureCache->GetRootSignature(rootSignatureResult.value);

	auto vertexShaderResult = m_device->CreateVertexShader(std::span<const std::byte>(vsBytecode->Data));
	auto pixelShaderResult = m_device->CreatePixelShader(std::span<const std::byte>(psBytecode->Data));
	if (!vertexShaderResult || !pixelShaderResult)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to create scene depth debug shader objects");
		m_failed = true;
		return false;
	}
	m_vertexShader = std::move(vertexShaderResult.value);
	m_pixelShader = std::move(pixelShaderResult.value);

	RHIPipelineStateLayout pipelineLayout{};
	pipelineLayout.vertexShader = m_vertexShader.get();
	pipelineLayout.pixelShader = m_pixelShader.get();
	pipelineLayout.inputLayout = InputLayoutType::None;
	pipelineLayout.depth = DepthStencilState::DepthNone;
	pipelineLayout.dsvFormat = DSV_FORMAT::Unknown;
	pipelineLayout.raster = RasterizerState::NoCull;
	pipelineLayout.rootSignature = layoutResult.value.Layout;
	pipelineLayout.DebugName = "SceneDepthDebug";

	auto pipelineStateResult = m_pipelineStateCache->GetOrCreatePipelineState(pipelineLayout);
	if (!pipelineStateResult)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to create the scene depth debug pipeline");
		m_failed = true;
		return false;
	}
	m_pipelineState = m_pipelineStateCache->GetPipelineState(pipelineStateResult.value);
	if (m_rootSignature == nullptr || m_pipelineState == nullptr)
	{
		LOG_ERROR(LogCategory::Renderer, "Scene depth debug pipeline objects are missing");
		m_failed = true;
		return false;
	}

	m_ready = true;
	return true;
}
