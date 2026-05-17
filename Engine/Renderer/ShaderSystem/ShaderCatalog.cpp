#include "Engine/Renderer/ShaderSystem/ShaderCatalog.h"

#include "Engine/Renderer/ShaderSystem/Internal/ShaderHash.h"

#include <algorithm>

namespace
{
	ShaderProgramDefinition MakeGraphicsProgram(
		const char* vertexSource,
		const char* vertexEntry,
		const char* pixelSource,
		const char* pixelEntry)
	{
		ShaderProgramDefinition program{};

		program.Stages[static_cast<std::size_t>(ShaderStage::Vertex)] = ShaderStageSource{
			.RelativeSourcePath = vertexSource,
			.EntryPoint = vertexEntry,
			.TargetProfile = "vs_6_0",
		};
		program.Stages[static_cast<std::size_t>(ShaderStage::Pixel)] = ShaderStageSource{
			.RelativeSourcePath = pixelSource,
			.EntryPoint = pixelEntry,
			.TargetProfile = "ps_6_0",
		};
		program.Permutations.push_back(ShaderPermutationDefinition{
			.Hash = ShaderPermutationHash{},
		});

		return program;
	}
}

ShaderCatalog::ShaderCatalog(std::filesystem::path assetsRoot)
	: m_assetsRoot(std::move(assetsRoot))
{
	RegisterBuiltInPrograms();
}

bool ShaderCatalog::IsValidPermutation(ShaderId id, ShaderPermutationHash permutation) const
{
	return TryGetPermutationDefinition(id, permutation) != nullptr;
}

ShaderPermutationHash ShaderCatalog::BuildPermutationHash(ShaderId id, std::span<const ShaderMacro> defines) const
{
	(void)TryGetProgramDefinition(id);
	return HashPermutationMacros(id, defines);
}

const ShaderProgramDefinition* ShaderCatalog::TryGetProgramDefinition(ShaderId id) const
{
	const auto it = m_programs.find(id);
	if (it == m_programs.end())
	{
		return nullptr;
	}
	return &it->second;
}

const ShaderPermutationDefinition* ShaderCatalog::TryGetPermutationDefinition(
	ShaderId id,
	ShaderPermutationHash permutation) const
{
	const ShaderProgramDefinition* program = TryGetProgramDefinition(id);
	if (program == nullptr)
	{
		return nullptr;
	}

	for (const ShaderPermutationDefinition& candidate : program->Permutations)
	{
		if (candidate.Hash == permutation)
		{
			return &candidate;
		}
	}

	return nullptr;
}

void ShaderCatalog::RegisterBuiltInPrograms()
{
	m_programs.emplace(
		ShaderId::Basic,
		MakeGraphicsProgram("Shaders/BasicVS.hlsl", "BasicVS", "Shaders/BasicPS.hlsl", "BasicPS"));
	m_programs.emplace(
		ShaderId::Fbx,
		MakeGraphicsProgram("Shaders/FbxVS.hlsl", "FbxVS", "Shaders/FbxPS.hlsl", "FbxPS"));
	m_programs.emplace(
		ShaderId::Sky,
		MakeGraphicsProgram("Shaders/SkyVS.hlsl", "SkyVS", "Shaders/SkyPS.hlsl", "SkyPS"));
}

ShaderPermutationHash ShaderCatalog::HashPermutationMacros(ShaderId id, std::span<const ShaderMacro> defines)
{
	std::uint64_t hash = ShaderHash::FnvOffsetBasis;
	hash = ShaderHash::Fnv1a64(hash, std::string_view(reinterpret_cast<const char*>(&id), sizeof(id)));

	std::vector<ShaderMacro> sortedDefines(defines.begin(), defines.end());
	std::ranges::sort(sortedDefines, [](const ShaderMacro& lhs, const ShaderMacro& rhs) {
		return lhs.Name < rhs.Name;
	});

	for (const ShaderMacro& define : sortedDefines)
	{
		hash = ShaderHash::Fnv1a64(hash, define.Name);
		hash = ShaderHash::Fnv1a64(hash, define.Value);
	}

	return ShaderPermutationHash{ hash };
}
