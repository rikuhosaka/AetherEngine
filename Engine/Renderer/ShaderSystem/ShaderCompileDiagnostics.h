#pragma once

#include <string>
#include <vector>

struct ShaderCompileDiagnostics
{
	std::string PrimaryErrorMessage{};
	std::vector<std::string> InfoAndWarnings{};
};
