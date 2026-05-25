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