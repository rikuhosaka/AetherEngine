#pragma once

#include <dxcapi.h>
#include <wrl/client.h>

class DxcShaderImpl
{
public:
	[[nodiscard]] bool Initialize(std::string& outError);

	Microsoft::WRL::ComPtr<IDxcUtils> utils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
};
