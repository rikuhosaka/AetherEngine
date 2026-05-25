#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderContext.h"

#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderImpl.h"

DxcShaderContext::DxcShaderContext()
	: m_impl(std::make_unique<DxcShaderImpl>())
{
}

std::unique_ptr<DxcShaderContext> DxcShaderContext::Create(std::string* outError)
{
	auto context = std::unique_ptr<DxcShaderContext>(new DxcShaderContext());

	std::string initError{};
	if (!context->m_impl->Initialize(initError))
	{
		if (outError != nullptr)
		{
			*outError = initError;
		}

		LOG_ERROR(initError.empty() ? "Failed to initialize DXC." : initError.c_str());
		return context;
	}

	context->m_initialized = true;
	return context;
}

DxcShaderImpl& DxcShaderContext::GetImpl() noexcept
{
	return *m_impl;
}

const DxcShaderImpl& DxcShaderContext::GetImpl() const noexcept
{
	return *m_impl;
}
