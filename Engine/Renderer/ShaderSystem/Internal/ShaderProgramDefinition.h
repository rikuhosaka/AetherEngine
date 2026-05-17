#pragma once

#include <array>

#include "Engine/Renderer/ShaderSystem/ShaderPermutationHash.h"
#include "Engine/Renderer/ShaderSystem/ShaderStage.h"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct ShaderStageSource
{
	std::filesystem::path RelativeSourcePath{};
	std::string EntryPoint{};
	std::string TargetProfile{};
};

struct ShaderPermutationDefinition
{
	ShaderPermutationHash Hash{};
	std::vector<std::pair<std::string, std::string>> Defines{};
};

struct ShaderProgramDefinition
{
	std::array<std::optional<ShaderStageSource>, static_cast<std::size_t>(ShaderStage::Count)> Stages{};
	std::vector<ShaderPermutationDefinition> Permutations{};
};
