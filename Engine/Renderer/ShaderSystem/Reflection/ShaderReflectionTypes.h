#pragma once

#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"
// Shader Resource
// ============================================================================

enum class ShaderResourceType
{
    Unknown,

    // ------------------------------------------------------------------------
    // Constant Buffer
    // ------------------------------------------------------------------------

    ConstantBuffer,

    // ------------------------------------------------------------------------
    // SRV Textures
    // ------------------------------------------------------------------------

    Texture1D,
    Texture1DArray,

    Texture2D,
    Texture2DArray,

    Texture2DMS,
    Texture2DMSArray,

    Texture3D,

    TextureCube,
    TextureCubeArray,

    // ------------------------------------------------------------------------
    // UAV Textures
    // ------------------------------------------------------------------------

    RWTexture1D,
    RWTexture1DArray,

    RWTexture2D,
    RWTexture2DArray,

    RWTexture3D,

    // ------------------------------------------------------------------------
    // Buffers
    // ------------------------------------------------------------------------

    StructuredBuffer,
    RWStructuredBuffer,

    ByteAddressBuffer,
    RWByteAddressBuffer,

    // ------------------------------------------------------------------------
    // Sampler
    // ------------------------------------------------------------------------

    Sampler,

    // ------------------------------------------------------------------------
    // Raytracing
    // ------------------------------------------------------------------------

    AccelerationStructure,
};

// ----------------------------------------------------------------------------

enum class ShaderResourceAccess : std::uint8_t
{
    ReadOnly,
    ReadWrite,
};

// ============================================================================
// Shader Variable
// ============================================================================

enum class ShaderVariableClass : std::uint8_t
{
    Unknown,

    Scalar,
    Vector,
    Matrix,

    Struct,
    Array,
};

// ----------------------------------------------------------------------------

enum class ShaderVariableType
{
    Unknown,

    // ------------------------------------------------------------------------
    // Bool
    // ------------------------------------------------------------------------

    Bool,

    // ------------------------------------------------------------------------
    // Integer
    // ------------------------------------------------------------------------

    Int,
    Int2,
    Int3,
    Int4,

    // ------------------------------------------------------------------------
    // Unsigned Integer
    // ------------------------------------------------------------------------

    UInt,
    UInt2,
    UInt3,
    UInt4,

    // ------------------------------------------------------------------------
    // Float
    // ------------------------------------------------------------------------

    Float,
    Float2,
    Float3,
    Float4,

    // ------------------------------------------------------------------------
    // Matrix
    // ------------------------------------------------------------------------

    Float2x2,
    Float3x3,
    Float4x4,
};

// ============================================================================
// Shader Input Layout
// ============================================================================

enum class ShaderInputClassification : std::uint8_t
{
    PerVertexData,
    PerInstanceData,
};

// ----------------------------------------------------------------------------

enum class ShaderInputType : std::uint8_t
{
    Unknown,

    Float,
    Float2,
    Float3,
    Float4,

    UInt,
    UInt2,
    UInt3,
    UInt4,

    Int,
    Int2,
    Int3,
    Int4,
};

// ============================================================================
// Shader Reflection Metadata
// ============================================================================

struct ShaderResourceBinding
{
    std::string Name{};

    ShaderResourceType Type =
        ShaderResourceType::Unknown;

    ShaderResourceAccess Access =
        ShaderResourceAccess::ReadOnly;

    ShaderStageFlags Visibility =
        ShaderStageFlags::None;

    std::uint32_t Register = 0;

    std::uint32_t Space = 0;

    std::uint32_t BindCount = 1;

    bool IsBindless = false;
};

// ----------------------------------------------------------------------------

struct ShaderConstantVariable
{
    std::string Name{};

    ShaderVariableType Type =
        ShaderVariableType::Unknown;

    ShaderVariableClass VariableClass =
        ShaderVariableClass::Unknown;

    std::uint32_t Offset = 0;

    std::uint32_t Size = 0;

    std::uint32_t AlignedSize = 0;

    std::uint32_t ArraySize = 0;

    std::vector<ShaderConstantVariable> Members{};
};

// ----------------------------------------------------------------------------

struct ShaderConstantBuffer
{
    std::string Name{};

    std::uint32_t Register = 0;

    std::uint32_t Space = 0;

    std::uint32_t Size = 0;

    std::uint32_t AlignedSize = 0;

    std::vector<ShaderConstantVariable> Variables{};
};

// ----------------------------------------------------------------------------

struct ShaderInputElement
{
    std::string SemanticName{};

    std::uint32_t SemanticIndex = 0;

    ShaderInputType Type =
        ShaderInputType::Unknown;

    std::uint32_t Register = 0;

    std::uint32_t Stream = 0;

    ShaderInputClassification Classification =
        ShaderInputClassification::PerVertexData;

    std::uint32_t InstanceStepRate = 0;
};

// ----------------------------------------------------------------------------

struct ComputeShaderReflection
{
    std::uint32_t ThreadGroupX = 1;

    std::uint32_t ThreadGroupY = 1;

    std::uint32_t ThreadGroupZ = 1;
};

// ----------------------------------------------------------------------------
