#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Core/ShaderSourcePolicy.h"
#include "Engine/Renderer/Material/MaterialBindCache.h"
#include "Engine/Renderer/ShaderSystem/Cache/ShaderCacheTypes.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"
#include "Engine/Renderer/Material/MaterialPool.h"
#include "Engine/Renderer/Material/MaterialTypes.h"

#include <filesystem>
#include <memory>
#include <span>
#include <string>

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
		TextureSystemServices* textureServices,
		const std::filesystem::path& compiledShaderRoot,
		ShaderSourcePolicy shaderSourcePolicy);

	[[nodiscard]] MaterialPool& GetPool() noexcept { return m_pool; }
	[[nodiscard]] MaterialBindCache& GetBindCache() noexcept { return m_bindCache; }
	[[nodiscard]] TextureSystemServices& GetTextureServices() const { return *m_textureServices; }
	[[nodiscard]] RHIDevice* GetDevice() const noexcept { return m_device; }

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
		TextureSystemServices* textureServices,
		std::filesystem::path compiledShaderRoot,
		ShaderSourcePolicy shaderSourcePolicy);

	[[nodiscard]] Result<ShaderBytecodeHandle> AcquireShaderBytecode(
		const std::filesystem::path& sourcePath,
		ShaderStage stage,
		const std::string& entryPointOverride);

	RHIDevice* m_device = nullptr;
	ShaderSystemServices* m_shaderServices = nullptr;
	RootSignatureCache* m_rootSignatureCache = nullptr;
	PipelineStateCache* m_pipelineStateCache = nullptr;
	TextureSystemServices* m_textureServices = nullptr;
	std::filesystem::path m_compiledShaderRoot{};
	ShaderSourcePolicy m_shaderSourcePolicy = ShaderSourcePolicy::PreferPrecompiled;

	MaterialPool m_pool{};
	MaterialBindCache m_bindCache;
};
