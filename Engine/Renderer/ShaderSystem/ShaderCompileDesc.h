#pragma once

#include "Engine/Renderer/ShaderSystem/ShaderType.h"


struct ShaderCompileDesc
{
    std::filesystem::path FilePath;

    std::string EntryPoint = "main";

    ShaderStage Stage = ShaderStage::Vertex;

    ShaderModel Model = ShaderModel::SM6_6;

    std::vector<ShaderDefine> Defines;

    std::vector<std::filesystem::path> IncludeDirectories;

    bool Debug = false;

    bool Optimization = true;

    bool TreatWarningsAsErrors = false;
};