#include "Engine/Renderer/Scene/RenderPlaceholders.h"

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Mesh/MeshUpload.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/Renderer/Texture/TextureUpload.h"
#include "Engine/RHI/Common/RHIInput.h"

#include <array>
#include <cstring>

namespace
{
struct PositionTexVertex
{
	float position[3];
	float uv[2];
};

struct SceneConstants
{
	float mvp[16];
};

struct MaterialConstants
{
	float tint[4];
};

void MakeIdentity(float* outMatrix4x4)
{
	std::memset(outMatrix4x4, 0, sizeof(float) * 16);
	outMatrix4x4[0] = 1.0f;
	outMatrix4x4[5] = 1.0f;
	outMatrix4x4[10] = 1.0f;
	outMatrix4x4[15] = 1.0f;
}
} // namespace

bool RenderPlaceholders::EnsureInitialized(
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot,
	std::string* outError)
{
	if (m_initialized)
	{
		return true;
	}

	if (commandList == nullptr)
	{
		if (outError != nullptr)
		{
			*outError = "RenderPlaceholders requires a valid command list";
		}
		return false;
	}

	const std::array<PositionTexVertex, 4> vertices = {
		PositionTexVertex{ { -0.5f, 0.5f, 0.0f }, { 0.0f, 0.0f } },
		PositionTexVertex{ { 0.5f, 0.5f, 0.0f }, { 1.0f, 0.0f } },
		PositionTexVertex{ { 0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } },
		PositionTexVertex{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } },
	};
	const std::array<uint32_t, 6> indices = { 0, 1, 2, 0, 2, 3 };

	MeshUploadDesc meshDesc{};
	meshDesc.vertices = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(vertices.data()),
		vertices.size() * sizeof(PositionTexVertex));
	meshDesc.indices = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(indices.data()),
		indices.size() * sizeof(uint32_t));
	meshDesc.vertexStride = sizeof(PositionTexVertex);
	meshDesc.vertexCount = static_cast<uint32_t>(vertices.size());
	meshDesc.indexCount = static_cast<uint32_t>(indices.size());
	meshDesc.indexFormat = IndexFormat::R32_UINT;
	meshDesc.layoutId = VertexLayoutId::PositionTex;

	auto meshResult = resources.GetMeshServices().UploadMesh(meshDesc, frameContext, commandList);
	if (!meshResult)
	{
		if (outError != nullptr)
		{
			*outError = meshResult.error.message;
		}
		return false;
	}
	m_resources.mesh = meshResult.value;

	const std::array<std::byte, 4> whitePixel = {
		std::byte{ 255 },
		std::byte{ 255 },
		std::byte{ 255 },
		std::byte{ 255 },
	};
	TextureUploadDesc textureDesc{};
	textureDesc.width = 1;
	textureDesc.height = 1;
	TextureMipData mip{};
	mip.width = 1;
	mip.height = 1;
	mip.rowPitch = 4;
	mip.pixels = whitePixel;
	textureDesc.mips.push_back(mip);

	auto textureResult = resources.GetTextureServices().UploadTexture(
		textureDesc,
		frameContext,
		commandList);
	if (!textureResult)
	{
		if (outError != nullptr)
		{
			*outError = textureResult.error.message;
		}
		return false;
	}
	m_resources.texture = textureResult.value;

	const std::array<std::byte, 4> flatNormalPixel = {
		std::byte{ 128 },
		std::byte{ 128 },
		std::byte{ 255 },
		std::byte{ 255 },
	};
	TextureUploadDesc normalTextureDesc{};
	normalTextureDesc.width = 1;
	normalTextureDesc.height = 1;
	TextureMipData normalMip{};
	normalMip.width = 1;
	normalMip.height = 1;
	normalMip.rowPitch = 4;
	normalMip.pixels = flatNormalPixel;
	normalTextureDesc.mips.push_back(normalMip);

	auto normalTextureResult = resources.GetTextureServices().UploadTexture(
		normalTextureDesc,
		frameContext,
		commandList);
	if (!normalTextureResult)
	{
		if (outError != nullptr)
		{
			*outError = normalTextureResult.error.message;
		}
		return false;
	}
	m_resources.normalTexture = normalTextureResult.value;

	MaterialCreateDesc materialDesc{};
	materialDesc.vertexShaderPath = shaderRoot / "SimpleVS.hlsl";
	materialDesc.pixelShaderPath = shaderRoot / "SimplePS.hlsl";
	materialDesc.vertexEntryPoint = "SimpleVS";
	materialDesc.pixelEntryPoint = "SimplePS";
	materialDesc.inputLayout = InputLayoutType::PositionTex;
	materialDesc.requiredLayout = VertexLayoutId::PositionTex;

	const Result<MaterialHandle> materialResult =
		resources.GetMaterialServices().CreateMaterial(materialDesc);
	if (!materialResult)
	{
		if (outError != nullptr)
		{
			*outError = materialResult.error.message;
		}
		return false;
	}
	m_resources.material = materialResult.value;

	SceneConstants sceneConstants{};
	MakeIdentity(sceneConstants.mvp);

	MaterialConstants materialConstants{};
	materialConstants.tint[0] = 1.0f;
	materialConstants.tint[1] = 1.0f;
	materialConstants.tint[2] = 1.0f;
	materialConstants.tint[3] = 1.0f;

	const std::array<std::span<const std::byte>, 2> constantBuffers = {
		std::span<const std::byte>(
			reinterpret_cast<const std::byte*>(&sceneConstants),
			sizeof(sceneConstants)),
		std::span<const std::byte>(
			reinterpret_cast<const std::byte*>(&materialConstants),
			sizeof(materialConstants)),
	};

	const std::array<TextureHandle, 1> textures = { m_resources.texture };
	m_resources.materialInstance = resources.GetMaterialServices().CreateInstance(
		m_resources.material,
		textures,
		constantBuffers);
	if (!m_resources.materialInstance.IsValid())
	{
		if (outError != nullptr)
		{
			*outError = "Failed to create placeholder material instance";
		}
		return false;
	}

	m_initialized = true;
	return true;
}
