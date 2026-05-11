#pragma once
#include "Engine/RHI/Interface/RHIShader.h"

#include <filesystem>


class ShaderImpl;

class DX12PixelShader : public RHIPixelShader
{
public:
	DX12PixelShader(const std::filesystem::path& filePath);


private:
	std::unique_ptr<ShaderImpl> m_impl = nullptr;

	ShaderImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12PipelineState;
};