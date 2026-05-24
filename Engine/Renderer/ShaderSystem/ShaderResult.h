#pragma once

struct ShaderBytecode
{
    std::vector<std::byte> Data;

    const void* GetPointer() const
    {
        return Data.data();
    }

    size_t GetSize() const
    {
        return Data.size();
    }
};

struct ShaderCompileResult
{
    bool Succeeded = false;

    ShaderBytecode Bytecode;

    std::string Errors;

    std::string Warnings;
};