#include "DX12PixelShader.h"
#include "Engine/RHI/DX12/Resource/ShaderImpl.h"


DX12PixelShader::DX12PixelShader(const std::filesystem::path& filePath)
	: m_impl(std::make_unique<ShaderImpl>())
{
	ComPtr<ID3DBlob> vs;
	
	HRESULT result = D3DReadFileToBlob(filePath.c_str(), &vs);
	if (FAILED(result))
	{
		LOG_ERROR("Failed to read pixel shader file: %s");
		return;
	}
	m_impl->blob = vs;
}