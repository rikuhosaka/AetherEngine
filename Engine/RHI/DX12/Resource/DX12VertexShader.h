#pragma once
#include "Engine/RHI/Interface/RHIShader.h"

#include <filesystem>


class ShaderImpl;

class DX12VertexShader : public RHIVertexShader
{
public:
	DX12VertexShader(const std::filesystem::path& filePath);

private:
	std::unique_ptr<ShaderImpl> m_impl = nullptr;

	ShaderImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12PipelineState;
};
