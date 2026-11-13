#pragma once

#include "Runtime/Core/Handle/Handle.h"
#include "Runtime/Renderer/Mesh/MeshTypes.h"
#include "Runtime/Renderer/Pipeline/ShaderRootLayoutTypes.h"
#include "Runtime/Renderer/Pipeline/RootSignatureCache.h"
#include "Runtime/Renderer/Pipeline/PipelineStateCache.h"
#include "Runtime/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"
#include "Runtime/Renderer/Texture/TextureTypes.h"
#include "Runtime/RHI/Common/RHIInput.h"
#include "Runtime/RHI/Common/RHIState.h"
#include "Runtime/RHI/Interface/RHIShader.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct MaterialTextureSlot
{
	std::string name{};
	uint32_t registerIndex = 0;
	uint32_t space = 0;
};

struct MaterialCreateDesc
{
	std::filesystem::path vertexShaderPath{};
	std::filesystem::path pixelShaderPath{};
	std::string vertexEntryPoint{};
	std::string pixelEntryPoint{};
	InputLayoutType inputLayout = InputLayoutType::PositionTex;
	VertexLayoutId requiredLayout = VertexLayoutId::PositionTex;
	BlendState blend = BlendState::Opaque;
	DepthStencilState depth = DepthStencilState::DepthDefault;
};

struct Material
{
	std::unique_ptr<RHIVertexShader> vertexShader{};
	std::unique_ptr<RHIPixelShader> pixelShader{};
	RootSignatureHandle rootSignature{};
	PipelineStateHandle pipelineState{};
	VertexLayoutId requiredLayout = VertexLayoutId::PositionTex;
	InputLayoutType inputLayout = InputLayoutType::PositionTex;

	std::vector<MaterialTextureSlot> textureSlots{};
	std::vector<ShaderConstantBuffer> constantLayout{};
	std::vector<ShaderRootBindingSlot> bindingSlots{};
	RHIRootSignatureLayout rootSignatureLayout{};
};

using MaterialHandle = Handle<Material>;

struct MaterialInstance
{
	MaterialHandle material{};
	std::vector<TextureHandle> boundTextures{};
	std::vector<std::vector<std::byte>> constantBuffers{};
};

using MaterialInstanceHandle = Handle<MaterialInstance>;
