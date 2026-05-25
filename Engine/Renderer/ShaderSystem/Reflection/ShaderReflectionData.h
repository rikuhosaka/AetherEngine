#pragma once

#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionTypes.h"


struct ShaderReflectionData
{
    ShaderStage Stage =
        ShaderStage::Unknown;

    std::vector<ShaderResourceBinding> Bindings{};

    std::vector<ShaderConstantBuffer> ConstantBuffers{};

    std::vector<ShaderInputElement> InputElements{};

    ComputeShaderReflection Compute{};
};
