#pragma once

#include "Engine/Renderer/ShaderSystem/ShaderCompileDesc.h"
#include "Engine/Renderer/ShaderSystem/ShaderResult.h"
class IShaderCompilerBackend;

class ShaderCompiler
{
public:

    explicit ShaderCompiler(
        std::unique_ptr<IShaderCompilerBackend> backend);

    ShaderCompileResult Compile(
        const ShaderCompileDesc& desc);

private:

    std::unique_ptr<IShaderCompilerBackend> Backend_;
};