#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderReflectionBackend.h"

#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderContext.h"
#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderImpl.h"
#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderReflectionConvert.h"
#include <d3d12shader.h>


using Microsoft::WRL::ComPtr;

DxcShaderReflectionBackend::DxcShaderReflectionBackend(DxcShaderContext* context)
	: m_context(context)
{
}

ShaderReflectionResult DxcShaderReflectionBackend::Reflect(
	const ShaderBytecode& bytecode,
	ShaderStage stage)
{
	ShaderReflectionResult result{};
	result.Data.Stage = stage;

	if (m_context == nullptr || !m_context->IsInitialized())
	{
		result.Errors = "DXC shader context is not initialized.";
		return result;
	}

	DxcShaderImpl& impl = m_context->GetImpl();
	if (impl.utils == nullptr)
	{
		result.Errors = "DXC utils is not initialized.";
		return result;
	}

	if (bytecode.Data.empty())
	{
		result.Errors = "Shader bytecode is empty.";
		return result;
	}

	DxcBuffer shaderBuffer{};
	shaderBuffer.Ptr = bytecode.GetPointer();
	shaderBuffer.Size = bytecode.GetSize();
	shaderBuffer.Encoding = DXC_CP_ACP;

	ComPtr<ID3D12ShaderReflection> d3dReflection;
	const HRESULT reflectionHr = impl.utils->CreateReflection(
		&shaderBuffer,
		IID_PPV_ARGS(d3dReflection.GetAddressOf()));
	if (FAILED(reflectionHr) || d3dReflection == nullptr)
	{
		result.Errors = "IDxcUtils::CreateReflection failed.";
		return result;
	}

	result.Data = ConvertD3D12ShaderReflection(d3dReflection.Get(), stage);
	result.Succeeded = true;
	return result;
}
