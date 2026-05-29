#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Interface/RHIShader.h"

#include <filesystem>
#include <memory>

class ShaderImpl;

class DX12PixelShader : public RHIPixelShader
{
public:
	~DX12PixelShader() override;

	[[nodiscard]] bool IsValid() const;

	static Result<std::unique_ptr<DX12PixelShader>> Create(std::span<const std::byte> bytecode);

private:
	explicit DX12PixelShader(const std::filesystem::path& filePath);
	explicit DX12PixelShader(std::span<const std::byte> bytecode);

	std::unique_ptr<ShaderImpl> m_impl = nullptr;

	ShaderImpl* GetImpl() const { return m_impl.get(); }

	friend class DX12PipelineState;
};
