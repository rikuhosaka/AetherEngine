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

Result<ShaderReflectionData> DxcShaderReflectionBackend::Reflect(
	const ShaderBytecode& bytecode,
	ShaderStage stage)
{
	if (m_context == nullptr || !m_context->IsInitialized())
	{
		return MakeFail<ShaderReflectionData>(
			ErrorCode::ShaderReflectionFailed,
			"DXC shader context is not initialized.");
	}

	DxcShaderImpl& impl = m_context->GetImpl();
	if (impl.utils == nullptr)
	{
		return MakeFail<ShaderReflectionData>(
			ErrorCode::ShaderReflectionFailed,
			"DXC utils is not initialized.");
	}

	if (bytecode.Data.empty())
	{
		return MakeFail<ShaderReflectionData>(
			ErrorCode::InvalidArgument,
			"Shader bytecode is empty.");
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
		return MakeFail<ShaderReflectionData>(
			ErrorCode::ShaderReflectionFailed,
			"IDxcUtils::CreateReflection failed.");
	}

	ShaderReflectionData data = ConvertD3D12ShaderReflection(d3dReflection.Get(), stage);
	data.Stage = stage;
	return MakeOk(std::move(data));
}
