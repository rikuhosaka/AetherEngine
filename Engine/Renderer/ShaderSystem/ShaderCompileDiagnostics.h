#pragma once


struct ShaderCompileDiagnostics
{
	std::string PrimaryErrorMessage{};
	std::vector<std::string> InfoAndWarnings{};
};
