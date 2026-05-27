#pragma once

#include "Engine/Core/Handle/Handle.h"
#include "Engine/Renderer/Mesh/MeshTypes.h"
#include "Engine/Renderer/Pipeline/ShaderRootLayoutTypes.h"
#include "Engine/Renderer/Pipeline/RootSignatureCache.h"
#include "Engine/Renderer/Pipeline/PipelineStateCache.h"
#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"
#include "Engine/Renderer/Texture/TextureTypes.h"
#include "Engine/RHI/Common/RHIInput.h"
#include "Engine/RHI/Interface/RHIShader.h"

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
