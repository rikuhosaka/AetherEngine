#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompiler.h"
#include "Engine/Renderer/ShaderSystem/Compiler/IShaderCompilerBackend.h"

ShaderCompiler::ShaderCompiler(std::unique_ptr<IShaderCompilerBackend> backend)
    : m_backend(std::move(backend))
    {
        if (m_backend == nullptr)
        {
            LOG_ERROR(LogCategory::Renderer, "Backend is not initialized");
            return;
        }
    }


ShaderCompiler::~ShaderCompiler()
{
    m_backend.reset();
}

ShaderCompileResult ShaderCompiler::Compile(const ShaderCompileDesc& desc)
{
    ShaderCompileResult result = m_backend->Compile(desc);
    if (!result.Succeeded)
    {
        LOG_ERROR(LogCategory::Renderer, result.Errors);
        return result;
    }
    return result;
}