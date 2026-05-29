#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHIShader.h"

#include <filesystem>
#include <memory>

class ShaderImpl;

class DX12VertexShader : public RHIVertexShader
{
public:
	~DX12VertexShader() override;

	[[nodiscard]] bool IsValid() const;

	static Result<std::unique_ptr<DX12VertexShader>> Create(std::span<const std::byte> bytecode);

private:
	explicit DX12VertexShader(const std::filesystem::path& filePath);
	explicit DX12VertexShader(std::span<const std::byte> bytecode);

	std::unique_ptr<ShaderImpl> m_impl = nullptr;

	ShaderImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12PipelineState;
};
