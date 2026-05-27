#pragma once

#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"

#include <memory>
#include <string>

class PipelineStateCache;
class RHIDescriptorAllocator;
class RHIDevice;
class RootSignatureCache;
class ShaderSystemServices;

class RenderResourceServices
{
public:
	static std::unique_ptr<RenderResourceServices> Create(
		RHIDevice* device,
		RHIDescriptorAllocator* descriptorAllocator,
		ShaderSystemServices* shaderServices,
		RootSignatureCache* rootSignatureCache,
		PipelineStateCache* pipelineStateCache,
		std::string* outError = nullptr);

	[[nodiscard]] MeshSystemServices& GetMeshServices() noexcept { return *m_meshServices; }
	[[nodiscard]] TextureSystemServices& GetTextureServices() noexcept { return *m_textureServices; }
	[[nodiscard]] MaterialSystemServices& GetMaterialServices() noexcept { return *m_materialServices; }

private:
	RenderResourceServices(
		std::unique_ptr<MeshSystemServices> meshServices,
		std::unique_ptr<TextureSystemServices> textureServices,
		std::unique_ptr<MaterialSystemServices> materialServices);

	std::unique_ptr<MeshSystemServices> m_meshServices{};
	std::unique_ptr<TextureSystemServices> m_textureServices{};
	std::unique_ptr<MaterialSystemServices> m_materialServices{};
};
