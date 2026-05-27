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

std::unique_ptr<RenderResourceServices> RenderResourceServices::Create(
	RHIDevice* device,
	RHIDescriptorAllocator* descriptorAllocator,
	ShaderSystemServices* shaderServices,
	RootSignatureCache* rootSignatureCache,
	PipelineStateCache* pipelineStateCache,
	std::string* outError)
{
	auto meshServices = MeshSystemServices::Create(device);
	auto textureServices = TextureSystemServices::Create(device, descriptorAllocator);
	auto materialServices = MaterialSystemServices::Create(
		device,
		shaderServices,
		rootSignatureCache,
		pipelineStateCache,
		textureServices.get(),
		outError);

	if (meshServices == nullptr || textureServices == nullptr || materialServices == nullptr)
	{
		if (outError != nullptr && outError->empty())
		{
			*outError = "Failed to create render resource services";
		}
		return nullptr;
	}

	return std::unique_ptr<RenderResourceServices>(new RenderResourceServices(
		std::move(meshServices),
		std::move(textureServices),
		std::move(materialServices)));
}
