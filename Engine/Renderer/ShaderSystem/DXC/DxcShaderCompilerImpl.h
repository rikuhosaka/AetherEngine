#pragma once

class DxcShaderCompilerImpl
{
public:
	[[nodiscard]] bool Initialize(std::string& outError);

	Microsoft::WRL::ComPtr<IDxcUtils> utils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
};