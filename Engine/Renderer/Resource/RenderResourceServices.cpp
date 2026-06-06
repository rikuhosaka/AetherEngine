#include "Engine/Renderer/Resource/RenderResourceServices.h"

#include "Engine/Renderer/Pipeline/PipelineStateCache.h"
#include "Engine/Renderer/Pipeline/RootSignatureCache.h"
#include "Engine/Renderer/ShaderSystem/ShaderSystemServices.h"

RenderResourceServices::RenderResourceServices(
	std::unique_ptr<MeshSystemServices> meshServices,
	std::unique_ptr<TextureSystemServices> textureServices,
	std::unique_ptr<MaterialSystemServices> materialServices)
	: m_meshServices(std::move(meshServices))
	, m_textureServices(std::move(textureServices))
	, m_materialServices(std::move(materialServices))
{
}

Result<std::unique_ptr<RenderResourceServices>> RenderResourceServices::Create(
	RHIDevice* device,
	RHIDescriptorAllocator* descriptorAllocator,
	ShaderSystemServices* shaderServices,
	RootSignatureCache* rootSignatureCache,
	PipelineStateCache* pipelineStateCache,
	const RendererConfig& rendererConfig)
{
	auto meshServicesResult = MeshSystemServices::Create(device);
	if (!meshServicesResult)
	{
		return MakeFail<std::unique_ptr<RenderResourceServices>>(
			meshServicesResult.error.code,
			meshServicesResult.error.message);
	}

	auto textureServicesResult = TextureSystemServices::Create(
		device,
		descriptorAllocator,
		rendererConfig.assetsRoot);
	if (!textureServicesResult)
	{
		return MakeFail<std::unique_ptr<RenderResourceServices>>(
			textureServicesResult.error.code,
			textureServicesResult.error.message);
	}

	auto materialServicesResult = MaterialSystemServices::Create(
		device,
		shaderServices,
		rootSignatureCache,
		pipelineStateCache,
		textureServicesResult.value.get(),
		rendererConfig.compiledShaderRoot,
		rendererConfig.shaderSourcePolicy);
	if (!materialServicesResult)
	{
		return MakeFail<std::unique_ptr<RenderResourceServices>>(
			materialServicesResult.error.code,
			materialServicesResult.error.message);
	}

	return MakeOk(std::unique_ptr<RenderResourceServices>(new RenderResourceServices(
		std::move(meshServicesResult.value),
		std::move(textureServicesResult.value),
		std::move(materialServicesResult.value))));
}
