#include "Engine/Renderer/Material/MaterialSystemServices.h"

#include "Engine/Core/Log/LogMacros.h"
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

#include <algorithm>
#include <cctype>

namespace
{
struct ResolvedShaderAsset
{
	std::filesystem::path path{};
	bool isPrecompiled = false;
};

std::string GetStemName(const std::filesystem::path& path)
{
	return path.stem().string();
}

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

Result<std::unique_ptr<MaterialSystemServices>> MaterialSystemServices::Create(
	RHIDevice* device,
	ShaderSystemServices* shaderServices,
	RootSignatureCache* rootSignatureCache,
	PipelineStateCache* pipelineStateCache,
	TextureSystemServices* textureServices,
	const std::filesystem::path& compiledShaderRoot,
	ShaderSourcePolicy shaderSourcePolicy)
{
	if (device == nullptr || shaderServices == nullptr || !shaderServices->IsInitialized())
	{
		return FailInternal<std::unique_ptr<MaterialSystemServices>>(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"ShaderSystemServices is not initialized");
	}
	if (rootSignatureCache == nullptr || pipelineStateCache == nullptr || textureServices == nullptr)
	{
		return FailInternal<std::unique_ptr<MaterialSystemServices>>(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Renderer caches are not initialized");
	}

	return MakeOk(std::unique_ptr<MaterialSystemServices>(new MaterialSystemServices(
		device,
		shaderServices,
		rootSignatureCache,
		pipelineStateCache,
		textureServices,
		compiledShaderRoot,
		shaderSourcePolicy)));
}

MaterialSystemServices::MaterialSystemServices(
	RHIDevice* device,
	ShaderSystemServices* shaderServices,
	RootSignatureCache* rootSignatureCache,
	PipelineStateCache* pipelineStateCache,
	TextureSystemServices* textureServices,
	std::filesystem::path compiledShaderRoot,
	ShaderSourcePolicy shaderSourcePolicy)
	: m_device(device)
	, m_shaderServices(shaderServices)
	, m_rootSignatureCache(rootSignatureCache)
	, m_pipelineStateCache(pipelineStateCache)
	, m_textureServices(textureServices)
	, m_compiledShaderRoot(std::move(compiledShaderRoot))
	, m_shaderSourcePolicy(shaderSourcePolicy)
	, m_bindCache(device)
{
}

Result<ShaderBytecodeHandle> MaterialSystemServices::AcquireShaderBytecode(
	const std::filesystem::path& sourcePath,
	ShaderStage stage,
	const std::string& entryPointOverride)
{
	const ResolvedShaderAsset asset = ResolveShaderAssetPath(
		sourcePath,
		m_compiledShaderRoot,
		m_shaderSourcePolicy);

	ShaderCompileDesc compileDesc{};
	compileDesc.FilePath = asset.path;
	compileDesc.Stage = stage;
	compileDesc.EntryPoint = entryPointOverride.empty()
		? GetStemName(sourcePath)
		: entryPointOverride;

	if (asset.isPrecompiled)
	{
		LOG_INFO(LogCategory::Renderer, "Using precompiled shader: " + asset.path.string());
	}

	return m_shaderServices->GetBytecodeCache().Acquire(compileDesc);
}

Result<MaterialHandle> MaterialSystemServices::CreateMaterial(const MaterialCreateDesc& desc)
{
	if (m_shaderServices == nullptr || m_device == nullptr)
	{
		return FailInternal<MaterialHandle>(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"MaterialSystemServices is not initialized");
	}

	ShaderBytecodeCache& bytecodeCache = m_shaderServices->GetBytecodeCache();
	ShaderReflectionCache& reflectionCache = m_shaderServices->GetReflectionCache();

	auto vsBytecodeResult = AcquireShaderBytecode(
		desc.vertexShaderPath,
		ShaderStage::Vertex,
		desc.vertexEntryPoint);
	if (!vsBytecodeResult)
	{
		return MakeFail<MaterialHandle>(vsBytecodeResult.error.code, vsBytecodeResult.error.message);
	}

	auto psBytecodeResult = AcquireShaderBytecode(
		desc.pixelShaderPath,
		ShaderStage::Pixel,
		desc.pixelEntryPoint);
	if (!psBytecodeResult)
	{
		return MakeFail<MaterialHandle>(psBytecodeResult.error.code, psBytecodeResult.error.message);
	}

	const ShaderBytecode* vsBytecode = bytecodeCache.GetBytecode(vsBytecodeResult.value);
	const ShaderBytecode* psBytecode = bytecodeCache.GetBytecode(psBytecodeResult.value);
	if (vsBytecode == nullptr || psBytecode == nullptr)
	{
		return FailRuntime<MaterialHandle>(
			LogCategory::Renderer,
			ErrorCode::ShaderCompileFailed,
			"Failed to resolve compiled material shader bytecode.");
	}

	auto vsReflectionResult =
		reflectionCache.GetOrReflect(vsBytecodeResult.value, bytecodeCache, ShaderStage::Vertex);
	if (!vsReflectionResult)
	{
		return MakeFail<MaterialHandle>(
			vsReflectionResult.error.code,
			vsReflectionResult.error.message);
	}

	auto psReflectionResult =
		reflectionCache.GetOrReflect(psBytecodeResult.value, bytecodeCache, ShaderStage::Pixel);
	if (!psReflectionResult)
	{
		return MakeFail<MaterialHandle>(
			psReflectionResult.error.code,
			psReflectionResult.error.message);
	}

	const ShaderReflectionData* vsReflection = reflectionCache.GetData(vsReflectionResult.value);
	const ShaderReflectionData* psReflection = reflectionCache.GetData(psReflectionResult.value);
	if (vsReflection == nullptr || psReflection == nullptr)
	{
		return FailRuntime<MaterialHandle>(
			LogCategory::Renderer,
			ErrorCode::ShaderReflectionFailed,
			"Failed to resolve reflected material shader data.");
	}

	ShaderRootLayoutBuilder layoutBuilder{};
	layoutBuilder.AddStage(*vsReflection);
	layoutBuilder.AddStage(*psReflection);
	auto layoutResult = layoutBuilder.Build();
	if (!layoutResult)
	{
		return MakeFail<MaterialHandle>(layoutResult.error.code, layoutResult.error.message);
	}

	auto rootSignatureResult =
		m_rootSignatureCache->GetOrCreateRootSignature(layoutResult.value.Layout);
	if (!rootSignatureResult)
	{
		return MakeFail<MaterialHandle>(
			rootSignatureResult.error.code,
			rootSignatureResult.error.message);
	}

	auto material = std::make_unique<Material>();
	auto vertexShaderResult = m_device->CreateVertexShader(
		std::span<const std::byte>(vsBytecode->Data));
	if (!vertexShaderResult)
	{
		return MakeFail<MaterialHandle>(
			vertexShaderResult.error.code,
			vertexShaderResult.error.message);
	}

	auto pixelShaderResult = m_device->CreatePixelShader(
		std::span<const std::byte>(psBytecode->Data));
	if (!pixelShaderResult)
	{
		return MakeFail<MaterialHandle>(
			pixelShaderResult.error.code,
			pixelShaderResult.error.message);
	}

	material->vertexShader = std::move(vertexShaderResult.value);
	material->pixelShader = std::move(pixelShaderResult.value);

	RHIPipelineStateLayout pipelineLayout{};
	pipelineLayout.vertexShader = material->vertexShader.get();
	pipelineLayout.pixelShader = material->pixelShader.get();
	pipelineLayout.inputLayout = desc.inputLayout;
	pipelineLayout.rootSignature = layoutResult.value.Layout;
	pipelineLayout.topology = PrimitiveTopology::TriangleList;

	auto pipelineStateResult = m_pipelineStateCache->GetOrCreatePipelineState(pipelineLayout);
	if (!pipelineStateResult)
	{
		return MakeFail<MaterialHandle>(
			pipelineStateResult.error.code,
			pipelineStateResult.error.message);
	}

	material->pipelineState = pipelineStateResult.value;
	material->rootSignature = rootSignatureResult.value;
	material->requiredLayout = desc.requiredLayout;
	material->inputLayout = desc.inputLayout;
	material->bindingSlots = layoutResult.value.Slots;
	material->rootSignatureLayout = layoutResult.value.Layout;
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

	return MakeOk(m_pool.AddMaterial(std::move(material)));
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
