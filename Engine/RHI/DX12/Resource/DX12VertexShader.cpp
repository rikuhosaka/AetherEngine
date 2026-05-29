#include "DX12VertexShader.h"
#include "Engine/RHI/DX12/Resource/ShaderImpl.h"

namespace
{
bool CreateBlobFromBytecode(std::span<const std::byte> bytecode, ComPtr<ID3DBlob>& outBlob)
{
	if (bytecode.empty())
	{
		return false;
	}
	const HRESULT hr = D3DCreateBlob(bytecode.size(), &outBlob);
	if (FAILED(hr))
	{
		return false;
	}
	std::memcpy(outBlob->GetBufferPointer(), bytecode.data(), bytecode.size());
	return true;
}
} // namespace

DX12VertexShader::DX12VertexShader(const std::filesystem::path& filePath)
	: m_impl(std::make_unique<ShaderImpl>())
{
	ComPtr<ID3DBlob> vs;
	const HRESULT result = D3DReadFileToBlob(filePath.c_str(), &vs);
	if (FAILED(result))
	{
		LOG_ERROR(LogCategory::RHI, "Failed to read vertex shader file");
		return;
	}
	m_impl->blob = vs;
}

DX12VertexShader::DX12VertexShader(std::span<const std::byte> bytecode)
	: m_impl(std::make_unique<ShaderImpl>())
{
	ComPtr<ID3DBlob> blob;
	if (!CreateBlobFromBytecode(bytecode, blob))
	{
		LOG_ERROR(LogCategory::RHI, "Failed to create vertex shader blob from bytecode");
		return;
	}
	m_impl->blob = blob;
}

DX12VertexShader::~DX12VertexShader() = default;
