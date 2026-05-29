#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderContext.h"

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderImpl.h"

DxcShaderContext::DxcShaderContext()
	: m_impl(std::make_unique<DxcShaderImpl>())
{
}

Result<std::unique_ptr<DxcShaderContext>> DxcShaderContext::Create()
{
	auto context = std::unique_ptr<DxcShaderContext>(new DxcShaderContext());

	std::string initError{};
	if (!context->m_impl->Initialize(initError))
	{
		return MakeFail<std::unique_ptr<DxcShaderContext>>(
			ErrorCode::ShaderCompileFailed,
			initError.empty() ? "Failed to initialize DXC." : initError);
	}

	context->m_initialized = true;
	return MakeOk(std::move(context));
}

DxcShaderImpl& DxcShaderContext::GetImpl() noexcept
{
	return *m_impl;
}

const DxcShaderImpl& DxcShaderContext::GetImpl() const noexcept
{
	return *m_impl;
}
