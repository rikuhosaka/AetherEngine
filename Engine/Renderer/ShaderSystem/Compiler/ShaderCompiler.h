#pragma once

#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderResult.h"
class IShaderCompilerBackend;

class ShaderCompiler
{
public:

    explicit ShaderCompiler(
        std::unique_ptr<IShaderCompilerBackend> backend);

    ~ShaderCompiler();

    ShaderCompileResult Compile(
        const ShaderCompileDesc& desc);

private:

    std::unique_ptr<IShaderCompilerBackend> m_backend;
};