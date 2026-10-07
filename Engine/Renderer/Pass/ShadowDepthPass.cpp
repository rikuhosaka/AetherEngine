#include "Engine/Renderer/Pass/ShadowDepthPass.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Pass/FrameConstantsBinder.h"
#include "Engine/Renderer/Pass/ObjectConstantsBinder.h"
#include "Engine/Renderer/Pipeline/PipelineStateCache.h"
#include "Engine/Renderer/Pipeline/RootSignatureCache.h"
#include "Engine/Renderer/Pipeline/ShaderRootLayoutBuilder.h"
#include "Engine/Renderer/Scene/ShadowMapConstants.h"
#include "Engine/Renderer/ShaderSystem/Cache/ShaderBytecodeCache.h"
#include "Engine/Renderer/ShaderSystem/Cache/ShaderReflectionCache.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"
#include "Engine/Renderer/ShaderSystem/ShaderSystemServices.h"
#include "Engine/RHI/Common/RHIPipelineStateLayout.h"
#include "Engine/RHI/Common/RHITexture.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Common/RHIScopedDebugEvent.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHIDSVAllocator.h"
#include "Engine/RHI/Interface/RHIShader.h"
#include "Engine/RHI/Interface/RHITexture.h"

#include <algorithm>
#include <cctype>

namespace
{
constexpr uint32_t kTriangleListTopology = 4;
constexpr float kShadowClearDepth = 1.0f;

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

ShadowDepthPass::~ShadowDepthPass() = default;

Result<void> ShadowDepthPass::Initialize(
	RHIDevice* device,
	ShaderSystemServices* shaderServices,
	RootSignatureCache* rootSignatureCache,
	PipelineStateCache* pipelineStateCache,
	const std::filesystem::path& shaderRoot,
	const std::filesystem::path& compiledShaderRoot,
	ShaderSourcePolicy shaderSourcePolicy)
{
	if (device == nullptr)
	{
		return FailInternal(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"ShadowDepthPass requires a device");
	}

	m_device = device;
	m_shaderServices = shaderServices;
	m_rootSignatureCache = rootSignatureCache;
	m_pipelineStateCache = pipelineStateCache;
	m_shaderRoot = shaderRoot;
	m_compiledShaderRoot = compiledShaderRoot;
	m_shaderSourcePolicy = shaderSourcePolicy;

	auto dsvAllocatorResult = device->CreateDSVAllocator(1);
	if (!dsvAllocatorResult)
	{
		return MakeFail(dsvAllocatorResult.error.code, dsvAllocatorResult.error.message);
	}
	m_dsvAllocator = std::move(dsvAllocatorResult.value);

	RHITextureDesc depthDesc{};
	depthDesc.Width = kShadowMapSize;
	depthDesc.Height = kShadowMapSize;
	depthDesc.Usage = ERHITextureUsage::DepthStencil;
	depthDesc.Format = ERHIFormat::R32_TYPELESS;
	depthDesc.depthStencilViewFormat = ERHIFormat::D32_FLOAT;
	depthDesc.shaderResourceViewFormat = ERHIFormat::R32_FLOAT;
	depthDesc.DebugName = "ShadowMap";

	auto textureResult = device->CreateTexture(depthDesc);
	if (!textureResult)
	{
		return MakeFail(textureResult.error.code, textureResult.error.message);
	}
	m_texture = std::move(textureResult.value);

	m_dsv = m_dsvAllocator->Allocate();
	device->WriteDepthStencilView(m_texture.get(), m_dsv);
	return MakeOk();
}

void ShadowDepthPass::Execute(
	FrameContext& frameContext,
	RHICommandList* commandList,
	const RenderFrameSnapshot& snapshot,
	MeshSystemServices& meshServices)
{
	if (commandList == nullptr || m_texture == nullptr || m_dsv.cpu.ptr == 0)
	{
		return;
	}

	const RHIScopedDebugEvent passEvent(commandList, "ShadowDepthPass");

	if (m_shaderReadable)
	{
		m_texture->TransitionResource(ERHIResourceState::DepthWrite, commandList);
		m_shaderReadable = false;
	}

	RtvHandle noColor{};
	commandList->OMSetRenderTargets(0, noColor, false, m_dsv);
	commandList->ClearDepthStencilView(m_dsv, kShadowClearDepth, 0);
	commandList->RSSetViewports(
		0.0f,
		0.0f,
		static_cast<float>(kShadowMapSize),
		static_cast<float>(kShadowMapSize));
	commandList->RSSetScissorRects(0, 0, static_cast<int>(kShadowMapSize), static_cast<int>(kShadowMapSize));

	if (EnsurePipeline() && snapshot.hasView && snapshot.hasLighting)
	{
		const FrameConstantsBinder frameBinder(m_device);
		const ObjectConstantsBinder objectBinder(m_device);

		commandList->SetPipelineState(m_pipelineState);
		commandList->SetRootSignature(m_rootSignature);
		commandList->IASetPrimitiveTopology(kTriangleListTopology);

		for (const RenderItem& item : snapshot.shadowItems)
		{
			Mesh* mesh = meshServices.GetMesh(item.mesh);
			if (mesh == nullptr || mesh->layoutId != VertexLayoutId::Basic
				|| mesh->vertexBuffer == nullptr || mesh->indexBuffer == nullptr)
			{
				continue;
			}

			if (item.submeshIndex >= mesh->submeshes.size()
				|| item.objectConstantsIndex >= snapshot.objectConstants.size())
			{
				continue;
			}

			if (!frameBinder.Bind(frameContext, commandList, m_material, snapshot.frameConstants))
			{
				LOG_ERROR(LogCategory::Renderer, "Failed to bind frame constants for shadow item");
				continue;
			}

			if (!objectBinder.Bind(
					frameContext,
					commandList,
					m_material,
					snapshot.objectConstants[item.objectConstantsIndex]))
			{
				LOG_ERROR(LogCategory::Renderer, "Failed to bind object constants for shadow item");
				continue;
			}

			const RHIVertexBuffer* vertexBuffers[] = { mesh->vertexBuffer.get() };
			commandList->IASetVertexBuffers(0, vertexBuffers);
			commandList->IASetIndexBuffer(mesh->indexBuffer.get());

			const SubmeshRange& submesh = mesh->submeshes[item.submeshIndex];
			commandList->DrawIndexedInstanced(submesh.indexCount, 1, submesh.indexStart, 0, 0);
		}
	}

	m_texture->TransitionResource(ERHIResourceState::PixelShaderResource, commandList);
	m_shaderReadable = true;
}

bool ShadowDepthPass::EnsurePipeline()
{
	if (m_pipelineReady)
	{
		return true;
	}
	if (m_pipelineFailed)
	{
		return false;
	}

	if (m_device == nullptr || m_shaderServices == nullptr || !m_shaderServices->IsInitialized()
		|| m_rootSignatureCache == nullptr || m_pipelineStateCache == nullptr || m_shaderRoot.empty())
	{
		LOG_ERROR(LogCategory::Renderer, "Shadow depth pass is not configured");
		m_pipelineFailed = true;
		return false;
	}

	const std::filesystem::path vertexPath = m_shaderRoot / "ShadowDepthVS.hlsl";
	const ResolvedShaderAsset asset = ResolveShaderAssetPath(
		vertexPath,
		m_compiledShaderRoot,
		m_shaderSourcePolicy);

	ShaderCompileDesc compileDesc{};
	compileDesc.FilePath = asset.path;
	compileDesc.Stage = ShaderStage::Vertex;
	compileDesc.EntryPoint = vertexPath.stem().string();
	if (!asset.isPrecompiled)
	{
		compileDesc.IncludeDirectories.push_back(vertexPath.parent_path());
	}

	auto bytecodeResult = m_shaderServices->GetBytecodeCache().Acquire(compileDesc);
	if (!bytecodeResult)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to load the shadow depth shader");
		m_pipelineFailed = true;
		return false;
	}

	ShaderBytecodeCache& bytecodeCache = m_shaderServices->GetBytecodeCache();
	ShaderReflectionCache& reflectionCache = m_shaderServices->GetReflectionCache();
	const ShaderBytecode* bytecode = bytecodeCache.GetBytecode(bytecodeResult.value);
	if (bytecode == nullptr)
	{
		LOG_ERROR(LogCategory::Renderer, "Shadow depth shader bytecode is missing");
		m_pipelineFailed = true;
		return false;
	}

	auto reflectionResult = reflectionCache.GetOrReflect(
		bytecodeResult.value,
		bytecodeCache,
		ShaderStage::Vertex);
	if (!reflectionResult)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to reflect the shadow depth shader");
		m_pipelineFailed = true;
		return false;
	}

	const ShaderReflectionData* reflection = reflectionCache.GetData(reflectionResult.value);
	if (reflection == nullptr)
	{
		LOG_ERROR(LogCategory::Renderer, "Shadow depth shader reflection is missing");
		m_pipelineFailed = true;
		return false;
	}

	ShaderRootLayoutBuilder layoutBuilder{};
	layoutBuilder.AddStage(*reflection);
	auto layoutResult = layoutBuilder.Build();
	if (!layoutResult)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to build the shadow depth root signature");
		m_pipelineFailed = true;
		return false;
	}

	auto rootSignatureResult = m_rootSignatureCache->GetOrCreateRootSignature(layoutResult.value.Layout);
	if (!rootSignatureResult)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to create the shadow depth root signature");
		m_pipelineFailed = true;
		return false;
	}

	auto vertexShaderResult = m_device->CreateVertexShader(std::span<const std::byte>(bytecode->Data));
	if (!vertexShaderResult)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to create the shadow depth vertex shader");
		m_pipelineFailed = true;
		return false;
	}

	m_material.vertexShader = std::move(vertexShaderResult.value);
	m_material.bindingSlots = layoutResult.value.Slots;
	m_material.rootSignatureLayout = layoutResult.value.Layout;
	m_material.constantLayout = reflection->ConstantBuffers;
	m_material.inputLayout = InputLayoutType::Basic;
	m_material.requiredLayout = VertexLayoutId::Basic;
	m_material.rootSignature = rootSignatureResult.value;

	RHIPipelineStateLayout pipelineLayout{};
	pipelineLayout.vertexShader = m_material.vertexShader.get();
	pipelineLayout.pixelShader = nullptr;
	pipelineLayout.inputLayout = InputLayoutType::Basic;
	pipelineLayout.numRT = 0;
	pipelineLayout.dsvFormat = DSV_FORMAT::D32_FLOAT;
	pipelineLayout.depth = DepthStencilState::DepthDefault;
	pipelineLayout.raster = RasterizerState::NoCull;
	pipelineLayout.rootSignature = layoutResult.value.Layout;
	pipelineLayout.DebugName = "ShadowDepth";

	auto pipelineStateResult = m_pipelineStateCache->GetOrCreatePipelineState(pipelineLayout);
	if (!pipelineStateResult)
	{
		LOG_ERROR(LogCategory::Renderer, "Failed to create the shadow depth pipeline");
		m_pipelineFailed = true;
		return false;
	}

	m_rootSignature = m_rootSignatureCache->GetRootSignature(rootSignatureResult.value);
	m_pipelineState = m_pipelineStateCache->GetPipelineState(pipelineStateResult.value);
	if (m_rootSignature == nullptr || m_pipelineState == nullptr)
	{
		LOG_ERROR(LogCategory::Renderer, "Shadow depth pipeline objects are missing");
		m_pipelineFailed = true;
		return false;
	}

	m_pipelineReady = true;
	return true;
}
