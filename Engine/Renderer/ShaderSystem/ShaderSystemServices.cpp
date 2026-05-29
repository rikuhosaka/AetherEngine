#include "Engine/Renderer/ShaderSystem/ShaderSystemServices.h"

#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderCompilerBackend.h"
#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderContext.h"
#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderImpl.h"
#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderReflectionBackend.h"

ShaderSystemServices::~ShaderSystemServices() = default;

Result<std::unique_ptr<ShaderSystemServices>> ShaderSystemServices::Create()
{
	auto services = std::unique_ptr<ShaderSystemServices>(new ShaderSystemServices());

	auto contextResult = DxcShaderContext::Create();
	if (!contextResult)
	{
		return MakeFail<std::unique_ptr<ShaderSystemServices>>(
			contextResult.error.code,
			contextResult.error.message);
	}
	services->m_context = std::move(contextResult.value);

	services->m_compilerBackend =
		std::make_unique<DxcShaderCompilerBackend>(services->m_context.get());
	services->m_reflectionBackend =
		std::make_unique<DxcShaderReflectionBackend>(services->m_context.get());

	services->m_bytecodeCache =
		std::make_unique<ShaderBytecodeCache>(services->m_compilerBackend.get());
	services->m_reflectionCache =
		std::make_unique<ShaderReflectionCache>(services->m_reflectionBackend.get());

	services->m_initialized = true;
	return MakeOk(std::move(services));
}

bool ShaderSystemServices::IsInitialized() const noexcept
{
	return m_initialized;
}

DxcShaderContext& ShaderSystemServices::GetContext() noexcept
{
	return *m_context;
}

const DxcShaderContext& ShaderSystemServices::GetContext() const noexcept
{
	return *m_context;
}

ShaderBytecodeCache& ShaderSystemServices::GetBytecodeCache() noexcept
{
	return *m_bytecodeCache;
}

const ShaderBytecodeCache& ShaderSystemServices::GetBytecodeCache() const noexcept
{
	return *m_bytecodeCache;
}

ShaderReflectionCache& ShaderSystemServices::GetReflectionCache() noexcept
{
	return *m_reflectionCache;
}

const ShaderReflectionCache& ShaderSystemServices::GetReflectionCache() const noexcept
{
	return *m_reflectionCache;
}

IShaderCompilerBackend& ShaderSystemServices::GetCompilerBackend() noexcept
{
	return *m_compilerBackend;
}

IShaderReflectionBackend& ShaderSystemServices::GetReflectionBackend() noexcept
{
	return *m_reflectionBackend;
}
