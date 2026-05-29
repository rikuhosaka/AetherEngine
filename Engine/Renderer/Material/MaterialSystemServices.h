#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Material/MaterialBindCache.h"
#include "Engine/Renderer/Material/MaterialPool.h"
#include "Engine/Renderer/Material/MaterialTypes.h"

#include <memory>
#include <span>

class PipelineStateCache;
class RootSignatureCache;
class ShaderSystemServices;
class TextureSystemServices;

class MaterialSystemServices
{
public:
	static Result<std::unique_ptr<MaterialSystemServices>> Create(
		RHIDevice* device,
		ShaderSystemServices* shaderServices,
		RootSignatureCache* rootSignatureCache,
		PipelineStateCache* pipelineStateCache,
		TextureSystemServices* textureServices);

	[[nodiscard]] MaterialPool& GetPool() noexcept { return m_pool; }
	[[nodiscard]] MaterialBindCache& GetBindCache() noexcept { return m_bindCache; }
	[[nodiscard]] TextureSystemServices& GetTextureServices() const { return *m_textureServices; }

	[[nodiscard]] Result<MaterialHandle> CreateMaterial(const MaterialCreateDesc& desc);

	[[nodiscard]] MaterialInstanceHandle CreateInstance(
		MaterialHandle material,
		std::span<const TextureHandle> textures,
		std::span<const std::span<const std::byte>> constantBuffers);

	[[nodiscard]] Material* GetMaterial(MaterialHandle handle) { return m_pool.GetMaterial(handle); }
	[[nodiscard]] MaterialInstance* GetInstance(MaterialInstanceHandle handle)
	{
		return m_pool.GetInstance(handle);
	}

private:
	MaterialSystemServices(
		RHIDevice* device,
		ShaderSystemServices* shaderServices,
		RootSignatureCache* rootSignatureCache,
		PipelineStateCache* pipelineStateCache,
		TextureSystemServices* textureServices);

	RHIDevice* m_device = nullptr;
	ShaderSystemServices* m_shaderServices = nullptr;
	RootSignatureCache* m_rootSignatureCache = nullptr;
	PipelineStateCache* m_pipelineStateCache = nullptr;
	TextureSystemServices* m_textureServices = nullptr;

	MaterialPool m_pool{};
	MaterialBindCache m_bindCache;
};
