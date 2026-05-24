#pragma once

#include "Engine/Renderer/ShaderSystem/IShaderCompilerBackend.h"


class DxcShaderCompilerImpl;

class DxcShaderCompilerBackend : public IShaderCompilerBackend
{
public:
    DxcShaderCompilerBackend();

    ShaderCompileResult Compile(
        const ShaderCompileDesc& desc) override;

private:
    std::unique_ptr<DxcShaderCompilerImpl> m_impl;
    DxcShaderCompilerImpl* GetImpl() const;

};