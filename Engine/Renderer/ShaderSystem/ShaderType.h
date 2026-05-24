#pragma once

enum class ShaderStage
{
    Vertex,
    Pixel,
    Compute,
    Geometry,
    Hull,
    Domain,
};

enum class ShaderModel
{
    SM6_0,
    SM6_1,
    SM6_2,
    SM6_3,
    SM6_4,
    SM6_5,
    SM6_6,
    SM6_7,
};

struct ShaderDefine
{
    std::string Name;
    std::string Value;
};