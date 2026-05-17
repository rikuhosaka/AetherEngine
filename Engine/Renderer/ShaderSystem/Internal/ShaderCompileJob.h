#pragma once

#include "Engine/Renderer/ShaderSystem/ShaderStage.h"

struct ShaderCompileJob
{
	ShaderStage Stage{};

	std::string SourcePath{};
	std::string EntryPoint{};
	std::string Profile{};

	std::vector<std::string> IncludeSearchPaths{};
	std::vector<std::pair<std::string, std::string>> Defines{};
};
