#pragma once

#include "Engine/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"

struct ShaderCompileResult
{
    bool Succeeded = false;

    ShaderBytecode Bytecode;

    std::string Errors;

    std::string Warnings;
};