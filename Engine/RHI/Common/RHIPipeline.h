#pragma once
#include "Engine/RHI/Common/RHIInput.h"
#include "Engine/RHI/Common/RHIState.h"
#include "Engine/RHI/Common/RHIFormat.h"

class RHIVertexShader;
class RHIPixelShader;
class RHIRootSignature;

struct RHIPipelineDesc
{
	// shader
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
	uint8_t numRT;
	DSV_FORMAT dsvFormat;

	// MSAA
	uint8_t sampleCount;

	// Root
	RHIRootSignature* rootSignature;

	// Optional label for GPU debug tools (PIX, RenderDoc). UTF-8.
	const char* DebugName = nullptr;
};