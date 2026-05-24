#include <dxcapi.h>
#include <wrl/client.h>

#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderCompilerImpl.h"


bool DxcShaderCompilerImpl::Initialize(std::string& outError)
{
	outError.clear();

	if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.GetAddressOf()))))
	{
		outError = "Failed to create IDxcUtils.";
		return false;
	}

	if (FAILED(utils->CreateDefaultIncludeHandler(includeHandler.GetAddressOf())))
	{
		outError = "Failed to create the default include handler.";
		return false;
	}

	if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.GetAddressOf()))))
	{
		outError = "Failed to create IDxcCompiler3.";
		return false;
	}

	return true;
}
