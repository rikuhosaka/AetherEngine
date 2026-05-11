#pragma once

enum class ERHIResourceState
{
	Common,
	VertexAndConstantBuffer,
	IndexBuffer,
	RenderTarget,
	UnorderedAccess,
	DepthWrite,
	DepthRead,
	NonPixelShaderResource,
	PixelShaderResource
};

enum class ERHITextureUsage
{
	RenderTarget,
	DepthStencil,
	ShaderResource,
	UnorderedAccess
};

enum class ERHIMemoryType
{
	Default,
	Upload,
	Readback
};

enum class IndexFormat
{
	R16_UINT,
	R32_UINT
};



struct RHIBufferDesc
{
    size_t Size = 0;
	ERHIMemoryType MemoryType;
};

struct RHITextureDesc
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	uint32_t MipLevels = 1;
	uint32_t ArraySize = 1;
	uint32_t SampleCount = 1;
	uint32_t SampleQuality = 0;
	ERHITextureUsage Usage;
};


struct CpuDescHandle { uint64_t ptr = 0; };
struct GpuDescHandle { uint64_t ptr = 0; };

struct CbvSrvUavHandle {
    CpuDescHandle cpu;
    GpuDescHandle gpu;
};

struct SamplerHandle {
    CpuDescHandle cpu;
    GpuDescHandle gpu;
};

struct RtvHandle {
    CpuDescHandle cpu; // GPU不要
};

struct DsvHandle {
    CpuDescHandle cpu; // GPU不要
};

// ====== CBV (b#) ======
constexpr uint32_t DRAW_INFO = 0;
constexpr uint32_t CBV_VIEWPROJ = 1;

// ====== SRV (t#) ======
constexpr uint32_t SRV_WORLD_MAT = 2;
constexpr uint32_t SRV_BONES = 3;
constexpr uint32_t SRV_MATERIAL = 4;
constexpr uint32_t SRV_TEXTURES = 5;

// ====== SAMPLER (s#) ======
constexpr uint32_t SAMPLER_LINEAR = 0;
constexpr uint32_t SAMPLER_ANISO = 1;
constexpr uint32_t SAMPLER_WRAP = 2;

class RHIVertexShader;
class RHIPixelShader;
class RHIRootSignature;

enum class InputLayoutType
{
	Basic,
	Skinned
};

enum class PrimitiveTopology
{
	TriangleList,
	TriangleStrip,
	LineList,
	PointList
};

enum class RasterizerState
{
	Solid,
	Wireframe,
	NoCull,
	FrontCull
};

enum class BlendState
{
	Opaque,
	AlphaBlend,
	Additive,
	NonPremultiplied
};

enum class DepthStencilState
{
	DepthDefault,
	DepthReadOnly,
	DepthNone,
	StencilDefault
};

enum class RTV_FORMAT
{
	R8G8B8A8_UNORM,
	R16G16B16A16_FLOAT,
	R32G32B32A32_FLOAT
};

enum class DSV_FORMAT
{
	D24_UNORM_S8_UINT,
	D32_FLOAT
};

struct RHIPipelineDesc {
	//shader
	RHIVertexShader* vertexShader;
	RHIPixelShader* pixelShader;

    // Input
    InputLayoutType inputLayout;
    PrimitiveTopology topology;

    // States
    RasterizerState raster;
    BlendState blend;
    DepthStencilState depth;

    // Output
    RTV_FORMAT rtvFormats[8];
    uint8_t     numRT;
    DSV_FORMAT dsvFormat;

    // MSAA
    uint8_t sampleCount;

    // Root
	RHIRootSignature* rootSignature;
};