#pragma once
#include "Engine/RHI/Interface/RHIShader.h"

#include <filesystem>


class ShaderImpl;

class DX12PixelShader : public RHIPixelShader
{
public:
	explicit DX12PixelShader(const std::filesystem::path& filePath);
	explicit DX12PixelShader(std::span<const std::byte> bytecode);
	~DX12PixelShader() override;

private:
	std::unique_ptr<ShaderImpl> m_impl = nullptr;

	ShaderImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12PipelineState;
};