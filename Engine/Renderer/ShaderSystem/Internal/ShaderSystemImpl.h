#pragma once

#include "Engine/Renderer/ShaderSystem/IShaderSystem.h"
#include "Engine/Renderer/ShaderSystem/Internal/IShaderCompilerBackend.h"
#include "Engine/Renderer/ShaderSystem/Internal/IShaderDiskCache.h"
#include "Engine/Renderer/ShaderSystem/Internal/ShaderCompileJob.h"
#include "Engine/Renderer/ShaderSystem/ShaderPipelineShaderIdentity.h"
#include "Engine/Renderer/ShaderSystem/ShaderSystemSettings.h"

#include <mutex>
#include <unordered_map>

class ShaderCacheKeyBuilder;
class ShaderCatalog;

class ShaderSystemImpl final : public IShaderSystem
{
public:
	ShaderSystemImpl(
		ShaderSystemSettings settings,
		const ShaderCatalog& catalog,
		std::unique_ptr<IShaderCompilerBackend> compiler,
		std::unique_ptr<IShaderDiskCache> diskCache);
	~ShaderSystemImpl() override;

	[[nodiscard]] std::optional<ShaderProgramBundle> TryGetProgram(
		ShaderId id,
		ShaderPermutationHash permutation) override;

	void RequestProgram(ShaderId id, ShaderPermutationHash permutation) override;

private:
	struct StoredStageBytecode
	{
		std::shared_ptr<const std::vector<std::byte>> Bytes{};
	};

	struct StoredProgram
	{
		std::array<std::optional<StoredStageBytecode>, static_cast<std::size_t>(ShaderStage::Count)> Stages{};
		ShaderProgramBundle Bundle{};
	};

	struct PipelineIdentityHasher
	{
		[[nodiscard]] std::size_t operator()(ShaderPipelineShaderIdentity identity) const noexcept
		{
			const std::size_t shaderHash = std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(identity.Shader));
			const std::size_t permutationHash = std::hash<std::uint64_t>{}(identity.Permutation.Value);
			return shaderHash ^ (permutationHash + 0x9E3779B97F4A7C15ULL + (shaderHash << 6) + (shaderHash >> 2));
		}
	};

	[[nodiscard]] bool CompileProgram(ShaderId id, ShaderPermutationHash permutation, StoredProgram& outProgram);

	[[nodiscard]] bool TryLoadPrebuiltStageBytecode(
		const std::filesystem::path& relativeSourcePath,
		std::vector<std::byte>& outBytecode) const;

	[[nodiscard]] ShaderCompileJob BuildCompileJob(
		ShaderStage stage,
		const struct ShaderStageSource& stageSource,
		const struct ShaderPermutationDefinition& permutation) const;

	ShaderSystemSettings m_settings{};
	const ShaderCatalog& m_catalog;
	std::unique_ptr<IShaderCompilerBackend> m_compiler;
	std::unique_ptr<IShaderDiskCache> m_diskCache;
	std::unique_ptr<ShaderCacheKeyBuilder> m_cacheKeyBuilder;

	std::mutex m_mutex{};
	std::unordered_map<ShaderPipelineShaderIdentity, StoredProgram, PipelineIdentityHasher> m_programs{};
};
