#pragma once

#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderResult.h"

class IShaderCompilerBackend
{
public:

    virtual ~IShaderCompilerBackend() = default;

    virtual ShaderCompileResult Compile(
        const ShaderCompileDesc& desc) = 0;
};