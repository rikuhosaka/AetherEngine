#pragma once

class DxcShaderImpl;

class DxcShaderContext
{
public:
	[[nodiscard]] static std::unique_ptr<DxcShaderContext> Create(std::string* outError = nullptr);

	[[nodiscard]] DxcShaderImpl& GetImpl() noexcept;
	[[nodiscard]] const DxcShaderImpl& GetImpl() const noexcept;

	[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
	DxcShaderContext();

	std::unique_ptr<DxcShaderImpl> m_impl;
	bool m_initialized{ false };
};
