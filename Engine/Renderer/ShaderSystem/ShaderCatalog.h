#pragma once

#include "Engine/Renderer/ShaderSystem/IShaderCatalog.h"
#include "Engine/Renderer/ShaderSystem/Internal/ShaderProgramDefinition.h"

#include <filesystem>
#include <unordered_map>

class ShaderCatalog final : public IShaderCatalog
{
public:
	explicit ShaderCatalog(std::filesystem::path assetsRoot);

	[[nodiscard]] bool IsValidPermutation(ShaderId id, ShaderPermutationHash permutation) const override;

	[[nodiscard]] ShaderPermutationHash BuildPermutationHash(ShaderId id, std::span<const ShaderMacro> defines) const override;

	[[nodiscard]] const ShaderProgramDefinition* TryGetProgramDefinition(ShaderId id) const;

	[[nodiscard]] const ShaderPermutationDefinition* TryGetPermutationDefinition(ShaderId id, ShaderPermutationHash permutation) const;

	[[nodiscard]] const std::filesystem::path& GetAssetsRoot() const noexcept { return m_assetsRoot; }

private:
	void RegisterBuiltInPrograms();

	[[nodiscard]] static ShaderPermutationHash HashPermutationMacros(ShaderId id, std::span<const ShaderMacro> defines);

	std::filesystem::path m_assetsRoot{};
	std::unordered_map<ShaderId, ShaderProgramDefinition> m_programs{};
};
