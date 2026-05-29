#include "Engine/Renderer/Material/MaterialSystemServices.h"

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Pipeline/PipelineStateCache.h"
#include "Engine/Renderer/Pipeline/RootSignatureCache.h"
#include "Engine/Renderer/Pipeline/ShaderRootLayoutBuilder.h"
#include "Engine/Renderer/ShaderSystem/ShaderSystemServices.h"
#include "Engine/Renderer/ShaderSystem/Cache/ShaderBytecodeCache.h"
#include "Engine/Renderer/ShaderSystem/Cache/ShaderReflectionCache.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/RHI/Common/RHIPipelineStateLayout.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHIShader.h"

namespace
{
std::string GetStemName(const std::filesystem::path& path)
{
	return path.stem().string();
}
} // namespace

std::unique_ptr<MaterialSystemServices> MaterialSystemServices::Create(
	RHIDevice* device,
	ShaderSystemServices* shaderServices,
	RootSignatureCache* rootSignatureCache,
	PipelineStateCache* pipelineStateCache,
	TextureSystemServices* textureServices,
	std::string* outError)
{
	if (device == nullptr || shaderServices == nullptr || !shaderServices->IsInitialized())
	{
		if (outError != nullptr)
		{
			*outError = "ShaderSystemServices is not initialized";
		}
		return nullptr;
	}
	if (rootSignatureCache == nullptr || pipelineStateCache == nullptr || textureServices == nullptr)
	{
		if (outError != nullptr)
		{
			*outError = "Renderer caches are not initialized";
		}
		return nullptr;
	}

	return std::unique_ptr<MaterialSystemServices>(new MaterialSystemServices(
		device,
		shaderServices,
		rootSignatureCache,
		pipelineStateCache,
		textureServices));
}

MaterialSystemServices::MaterialSystemServices(
	RHIDevice* device,
	ShaderSystemServices* shaderServices,
	RootSignatureCache* rootSignatureCache,
	PipelineStateCache* pipelineStateCache,
	TextureSystemServices* textureServices)
	: m_device(device)
	, m_shaderServices(shaderServices)
	, m_rootSignatureCache(rootSignatureCache)
	, m_pipelineStateCache(pipelineStateCache)
	, m_textureServices(textureServices)
	, m_bindCache(device)
{
}

std::optional<MaterialHandle> MaterialSystemServices::CreateMaterial(
	const MaterialCreateDesc& desc,
	std::string* outError)
{
	if (m_shaderServices == nullptr || m_device == nullptr)
	{
		return std::nullopt;
	}

	ShaderCompileDesc vsDesc{};
	vsDesc.FilePath = desc.vertexShaderPath;
	vsDesc.Stage = ShaderStage::Vertex;
	vsDesc.EntryPoint = desc.vertexEntryPoint.empty()
		? GetStemName(desc.vertexShaderPath)
		: desc.vertexEntryPoint;

	ShaderCompileDesc psDesc = vsDesc;
	psDesc.FilePath = desc.pixelShaderPath;
	psDesc.Stage = ShaderStage::Pixel;
	psDesc.EntryPoint = desc.pixelEntryPoint.empty()
		? GetStemName(desc.pixelShaderPath)
		: desc.pixelEntryPoint;

	ShaderBytecodeCache& bytecodeCache = m_shaderServices->GetBytecodeCache();
	ShaderReflectionCache& reflectionCache = m_shaderServices->GetReflectionCache();

	const ShaderBytecodeHandle vsBytecodeHandle = bytecodeCache.GetOrCompile(vsDesc);
	const ShaderBytecodeHandle psBytecodeHandle = bytecodeCache.GetOrCompile(psDesc);
	const ShaderBytecode* vsBytecode = bytecodeCache.GetBytecode(vsBytecodeHandle);
	const ShaderBytecode* psBytecode = bytecodeCache.GetBytecode(psBytecodeHandle);
	if (vsBytecode == nullptr || psBytecode == nullptr)
	{
		if (outError != nullptr)
		{
			*outError = "Failed to compile material shaders";
		}
		return std::nullopt;
	}

	const ShaderReflectionHandle vsReflectionHandle =
		reflectionCache.GetOrReflect(vsBytecodeHandle, bytecodeCache, ShaderStage::Vertex);
	const ShaderReflectionHandle psReflectionHandle =
		reflectionCache.GetOrReflect(psBytecodeHandle, bytecodeCache, ShaderStage::Pixel);
	const ShaderReflectionData* vsReflection = reflectionCache.GetData(vsReflectionHandle);
	const ShaderReflectionData* psReflection = reflectionCache.GetData(psReflectionHandle);
	if (vsReflection == nullptr || psReflection == nullptr)
	{
		if (outError != nullptr)
		{
			*outError = "Failed to reflect material shaders";
		}
		return std::nullopt;
	}

	ShaderRootLayoutBuilder layoutBuilder{};
	layoutBuilder.AddStage(*vsReflection);
	layoutBuilder.AddStage(*psReflection);
	const ShaderRootLayoutBuildResult layoutResult = layoutBuilder.Build();
	if (!layoutResult.Success)
	{
		if (outError != nullptr)
		{
			*outError = layoutResult.Error;
		}
		return std::nullopt;
	}

	const RootSignatureHandle rootSignatureHandle =
		m_rootSignatureCache->GetOrCreateRootSignature(layoutResult.Layout);
	if (!rootSignatureHandle.IsValid())
	{
		if (outError != nullptr)
		{
			*outError = "Failed to create root signature for material";
		}
		return std::nullopt;
	}

	auto material = std::make_unique<Material>();
	auto vertexShaderResult = m_device->CreateVertexShader(
		std::span<const std::byte>(vsBytecode->Data));
	auto pixelShaderResult = m_device->CreatePixelShader(
		std::span<const std::byte>(psBytecode->Data));
	if (!vertexShaderResult || !pixelShaderResult)
	{
		if (!vertexShaderResult)
		{
			LogResult(vertexShaderResult, LogCategory::Renderer);
		}
		if (!pixelShaderResult)
		{
			LogResult(pixelShaderResult, LogCategory::Renderer);
		}
		if (outError != nullptr)
		{
			*outError = "Failed to create RHI shaders for material";
		}
		return std::nullopt;
	}
	material->vertexShader = std::move(vertexShaderResult.value);
	material->pixelShader = std::move(pixelShaderResult.value);

	RHIPipelineStateLayout pipelineLayout{};
	pipelineLayout.vertexShader = material->vertexShader.get();
	pipelineLayout.pixelShader = material->pixelShader.get();
	pipelineLayout.inputLayout = desc.inputLayout;
	pipelineLayout.rootSignature = layoutResult.Layout;
	pipelineLayout.topology = PrimitiveTopology::TriangleList;

	material->pipelineState = m_pipelineStateCache->GetOrCreatePipelineState(pipelineLayout);
	if (!material->pipelineState.IsValid())
	{
		if (outError != nullptr)
		{
			*outError = "Failed to create pipeline state for material";
		}
		return std::nullopt;
	}

	material->rootSignature = rootSignatureHandle;
	material->requiredLayout = desc.requiredLayout;
	material->inputLayout = desc.inputLayout;
	material->bindingSlots = layoutResult.Slots;
	material->rootSignatureLayout = layoutResult.Layout;
	material->constantLayout = vsReflection->ConstantBuffers;
	material->constantLayout.insert(
		material->constantLayout.end(),
		psReflection->ConstantBuffers.begin(),
		psReflection->ConstantBuffers.end());

	for (const ShaderResourceBinding& binding : vsReflection->Bindings)
	{
		if (binding.Type == ShaderResourceType::Texture2D)
		{
			MaterialTextureSlot slot{};
			slot.name = binding.Name;
			slot.registerIndex = binding.Register;
			slot.space = binding.Space;
			material->textureSlots.push_back(slot);
		}
	}
	for (const ShaderResourceBinding& binding : psReflection->Bindings)
	{
		if (binding.Type == ShaderResourceType::Texture2D)
		{
			MaterialTextureSlot slot{};
			slot.name = binding.Name;
			slot.registerIndex = binding.Register;
			slot.space = binding.Space;
			material->textureSlots.push_back(slot);
		}
	}

	return m_pool.AddMaterial(std::move(material));
}

MaterialInstanceHandle MaterialSystemServices::CreateInstance(
	MaterialHandle material,
	std::span<const TextureHandle> textures,
	std::span<const std::span<const std::byte>> constantBuffers)
{
	auto instance = std::make_unique<MaterialInstance>();
	instance->material = material;
	instance->boundTextures.assign(textures.begin(), textures.end());
	instance->constantBuffers.reserve(constantBuffers.size());
	for (std::span<const std::byte> constantBuffer : constantBuffers)
	{
		instance->constantBuffers.emplace_back(constantBuffer.begin(), constantBuffer.end());
	}
	return m_pool.AddInstance(std::move(instance));
}
