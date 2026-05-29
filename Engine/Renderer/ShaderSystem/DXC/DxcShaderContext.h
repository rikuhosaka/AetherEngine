#pragma once

#include "Engine/Core/Log/Result.h"

#include <memory>

class DxcShaderImpl;

class DxcShaderContext
{
public:
	[[nodiscard]] static Result<std::unique_ptr<DxcShaderContext>> Create();

	[[nodiscard]] DxcShaderImpl& GetImpl() noexcept;
	[[nodiscard]] const DxcShaderImpl& GetImpl() const noexcept;

	[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
	DxcShaderContext();

	std::unique_ptr<DxcShaderImpl> m_impl;
	bool m_initialized{ false };
};
