#include "Engine/Renderer/ShaderSystem/Internal/ShaderSystemImpl.h"

#include "Engine/Renderer/ShaderSystem/Internal/ShaderCacheKeyBuilder.h"
#include "Engine/Renderer/ShaderSystem/Internal/ShaderProgramDefinition.h"
#include "Engine/Renderer/ShaderSystem/ShaderCatalog.h"
#include "Engine/Renderer/ShaderSystem/ShaderPipelineShaderIdentity.h"

#include <fstream>

namespace
{
	[[nodiscard]] bool ReadFileBytes(const std::filesystem::path& path, std::vector<std::byte>& outBytes)
	{
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
		{
			return false;
		}

		stream.seekg(0, std::ios::end);
		const std::streamoff size = stream.tellg();
		if (size <= 0)
		{
			return false;
		}

		outBytes.resize(static_cast<std::size_t>(size));
		stream.seekg(0, std::ios::beg);
		stream.read(reinterpret_cast<char*>(outBytes.data()), static_cast<std::streamsize>(outBytes.size()));
		return static_cast<bool>(stream);
	}
}

ShaderSystemImpl::ShaderSystemImpl(
	ShaderSystemSettings settings,
	const ShaderCatalog& catalog,
	std::unique_ptr<IShaderCompilerBackend> compiler,
	std::unique_ptr<IShaderDiskCache> diskCache)
	: m_settings(std::move(settings))
	, m_catalog(catalog)
	, m_compiler(std::move(compiler))
	, m_diskCache(std::move(diskCache))
	, m_cacheKeyBuilder(std::make_unique<ShaderCacheKeyBuilder>(ShaderCacheKeyBuilderOptions{
		.AssetsRoot = m_settings.AssetsRoot,
		.CompilerIdentity = m_settings.DxcExecutable.string(),
	}))
{
	std::error_code errorCode{};
	if (!m_settings.ScratchDirectory.empty())
	{
		std::filesystem::create_directories(m_settings.ScratchDirectory, errorCode);
	}
}

ShaderSystemImpl::~ShaderSystemImpl() = default;

std::optional<ShaderProgramBundle> ShaderSystemImpl::TryGetProgram(
	ShaderId id,
	ShaderPermutationHash permutation)
{
	const ShaderPipelineShaderIdentity identity{ id, permutation };

	std::scoped_lock lock(m_mutex);
	const auto it = m_programs.find(identity);
	if (it == m_programs.end())
	{
		return std::nullopt;
	}

	return it->second.Bundle;
}

void ShaderSystemImpl::RequestProgram(ShaderId id, ShaderPermutationHash permutation)
{
	const ShaderPipelineShaderIdentity identity{ id, permutation };

	{
		std::scoped_lock lock(m_mutex);
		if (m_programs.contains(identity))
		{
			return;
		}
	}

	if (!m_catalog.IsValidPermutation(id, permutation))
	{
		LOG_WARN("Requested shader permutation is not registered in the catalog.");
		return;
	}

	StoredProgram storedProgram{};
	if (!CompileProgram(id, permutation, storedProgram))
	{
		LOG_ERROR("Failed to compile shader program.");
		return;
	}

	std::scoped_lock lock(m_mutex);
	m_programs.emplace(identity, std::move(storedProgram));
}

bool ShaderSystemImpl::CompileProgram(
	ShaderId id,
	ShaderPermutationHash permutation,
	StoredProgram& outProgram)
{
	const ShaderProgramDefinition* programDefinition = m_catalog.TryGetProgramDefinition(id);
	const ShaderPermutationDefinition* permutationDefinition =
		m_catalog.TryGetPermutationDefinition(id, permutation);

	if (programDefinition == nullptr || permutationDefinition == nullptr)
	{
		return false;
	}

	for (std::size_t stageIndex = 0; stageIndex < programDefinition->Stages.size(); ++stageIndex)
	{
		const std::optional<ShaderStageSource>& stageSource = programDefinition->Stages[stageIndex];
		if (!stageSource.has_value())
		{
			continue;
		}

		const ShaderStage stage = static_cast<ShaderStage>(stageIndex);
		std::vector<std::byte> bytecode{};

		if (TryLoadPrebuiltStageBytecode(stageSource->RelativeSourcePath, bytecode))
		{
			// Development fast path: use CMake-generated CSO files when available.
		}
		else
		{
			ShaderCompileJob compileJob = BuildCompileJob(stage, *stageSource, *permutationDefinition);
			const ShaderCacheKey cacheKey = m_cacheKeyBuilder->Build(compileJob);

			if (m_diskCache != nullptr && m_diskCache->TryRead(cacheKey, bytecode))
			{
				// Loaded from persistent cache.
			}
			else
			{
				ShaderCompileOutput compileOutput{};
				if (m_compiler == nullptr || !m_compiler->Compile(compileJob, compileOutput))
				{
					if (!compileOutput.Diagnostics.PrimaryErrorMessage.empty())
					{
						LOG_ERROR(compileOutput.Diagnostics.PrimaryErrorMessage.c_str());
					}
					return false;
				}

				bytecode = std::move(compileOutput.Bytecode);
				if (m_diskCache != nullptr)
				{
					m_diskCache->Write(cacheKey, bytecode);
				}
			}
		}

		auto storage = std::make_shared<std::vector<std::byte>>(std::move(bytecode));
		StoredStageBytecode storedStage{
			.Bytes = storage,
		};

		outProgram.Stages[stageIndex] = storedStage;
		outProgram.Bundle.Stages[stageIndex] = ShaderBytecodeView{
			.Bytes = std::span<const std::byte>(storage->data(), storage->size()),
		};
	}

	return true;
}

bool ShaderSystemImpl::TryLoadPrebuiltStageBytecode(
	const std::filesystem::path& relativeSourcePath,
	std::vector<std::byte>& outBytecode) const
{
	if (m_settings.CompiledShadersDirectory.empty())
	{
		return false;
	}

	const std::filesystem::path prebuiltPath =
		m_settings.CompiledShadersDirectory / (relativeSourcePath.stem().string() + ".cso");
	return ReadFileBytes(prebuiltPath, outBytecode);
}

ShaderCompileJob ShaderSystemImpl::BuildCompileJob(
	ShaderStage stage,
	const ShaderStageSource& stageSource,
	const ShaderPermutationDefinition& permutation) const
{
	const std::filesystem::path absoluteSource = m_settings.AssetsRoot / stageSource.RelativeSourcePath;

	ShaderCompileJob job{};
	job.Stage = stage;
	job.SourcePath = absoluteSource.string();
	job.EntryPoint = stageSource.EntryPoint;
	job.Profile = stageSource.TargetProfile;
	job.IncludeSearchPaths = { (m_settings.AssetsRoot / "Shaders").string() };
	job.Defines = permutation.Defines;
	return job;
}
