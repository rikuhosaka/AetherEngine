#pragma once

enum class ShaderStage
{
    Unknown,
    Vertex,
    Pixel,
    Compute,
    Geometry,
    Hull,
    Domain,
};

enum class ShaderStageFlags : std::uint32_t 
{ 
    None = 0,

    Vertex = 1 << 0, 
    Pixel = 1 << 1, 
    Compute = 1 << 2, 
    Geometry = 1 << 3, 
    Hull = 1 << 4, 
    Domain = 1 << 5, 

    AllGraphics = 
    Vertex | 
    Pixel | 
    Geometry | 
    Hull | 
    Domain,

    All = 0xFFFFFFFF 
};

// ----------------------------------------------------------------------------

inline ShaderStageFlags operator|(
    ShaderStageFlags lhs,
    ShaderStageFlags rhs)
{
    return static_cast<ShaderStageFlags>(
        static_cast<std::uint32_t>(lhs) |
        static_cast<std::uint32_t>(rhs));
}

// ----------------------------------------------------------------------------

inline ShaderStageFlags operator&(
    ShaderStageFlags lhs,
    ShaderStageFlags rhs)
{
    return static_cast<ShaderStageFlags>(
        static_cast<std::uint32_t>(lhs) &
        static_cast<std::uint32_t>(rhs));
}

// ----------------------------------------------------------------------------

inline ShaderStageFlags& operator|=(
    ShaderStageFlags& lhs,
    ShaderStageFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

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