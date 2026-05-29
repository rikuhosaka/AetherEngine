#include "DX12PixelShader.h"
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

DX12PixelShader::DX12PixelShader(const std::filesystem::path& filePath)
	: m_impl(std::make_unique<ShaderImpl>())
{
	ComPtr<ID3DBlob> ps;
	const HRESULT result = D3DReadFileToBlob(filePath.c_str(), &ps);
	if (FAILED(result))
	{
		LOG_ERROR(LogCategory::RHI, "Failed to read pixel shader file");
		return;
	}
	m_impl->blob = ps;
}

DX12PixelShader::DX12PixelShader(std::span<const std::byte> bytecode)
	: m_impl(std::make_unique<ShaderImpl>())
{
	ComPtr<ID3DBlob> blob;
	if (!CreateBlobFromBytecode(bytecode, blob))
	{
		LOG_ERROR(LogCategory::RHI, "Failed to create pixel shader blob from bytecode");
		return;
	}
	m_impl->blob = blob;
}

DX12PixelShader::~DX12PixelShader() = default;
