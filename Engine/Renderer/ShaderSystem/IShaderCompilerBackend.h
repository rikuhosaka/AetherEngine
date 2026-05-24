#pragma once

#include "Engine/Renderer/ShaderSystem/ShaderCompileDesc.h"
#include "Engine/Renderer/ShaderSystem/ShaderResult.h"

class IShaderCompilerBackend
{
public:

    virtual ~IShaderCompilerBackend() = default;

    virtual ShaderCompileResult Compile(
        const ShaderCompileDesc& desc) = 0;
};