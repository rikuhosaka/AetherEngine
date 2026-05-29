#include "Engine/Renderer/Test/RendererResourceSmokeTest.h"

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Mesh/MeshUpload.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
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

void MakeIdentity(float* outMatrix4x4)
{
	std::memset(outMatrix4x4, 0, sizeof(float) * 16);
	outMatrix4x4[0] = 1.0f;
	outMatrix4x4[5] = 1.0f;
	outMatrix4x4[10] = 1.0f;
	outMatrix4x4[15] = 1.0f;
}

void FillExtractedObjectOverrides(
	ExtractedObject& object,
	const TextureHandle& textureHandle,
	const RendererResourceSmokeSceneConstants& sceneConstants,
	const RendererResourceSmokeMaterialConstants& materialConstants)
{
	object.overrides.baseColor = textureHandle;
	object.overrides.normal = {};

	object.overrides.parameters.clear();
	object.overrides.parameters.reserve(2);

	MaterialParameterBlock sceneBlock{};
	sceneBlock.bindingSlot = 0;
	sceneBlock.data = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(&sceneConstants),
		sizeof(sceneConstants));
	object.overrides.parameters.push_back(sceneBlock);

	MaterialParameterBlock materialBlock{};
	materialBlock.bindingSlot = 1;
	materialBlock.data = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(&materialConstants),
		sizeof(materialConstants));
	object.overrides.parameters.push_back(materialBlock);
}
} // namespace

RendererResourceSmokeResult BuildRendererResourceSmokeScene(
	Renderer& renderer,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot)
{
	RendererResourceSmokeResult result{};

	RenderResourceServices* resources = renderer.GetResourceServices();
	if (resources == nullptr || commandList == nullptr)
	{
		result.error = "Renderer resource services are not initialized";
		return result;
	}

	const std::array<PositionTexVertex, 3> vertices = {
		PositionTexVertex{ { 0.0f, 0.5f, 0.0f }, { 0.5f, 0.0f } },
		PositionTexVertex{ { 0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } },
		PositionTexVertex{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } },
	};
	const std::array<uint32_t, 3> indices = { 0, 1, 2 };

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

	const Result<MeshHandle> meshResult = resources->GetMeshServices().UploadMesh(
		meshDesc,
		frameContext,
		commandList);
	if (!meshResult)
	{
		result.error = meshResult.error.message;
		return result;
	}
	const MeshHandle meshHandle = meshResult.value;

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

	const Result<TextureHandle> textureResult = resources->GetTextureServices().UploadTexture(
		textureDesc,
		frameContext,
		commandList);
	if (!textureResult)
	{
		result.error = textureResult.error.message;
		return result;
	}
	const TextureHandle textureHandle = textureResult.value;

	MaterialCreateDesc materialDesc{};
	materialDesc.vertexShaderPath = shaderRoot / "SimpleVS.hlsl";
	materialDesc.pixelShaderPath = shaderRoot / "SimplePS.hlsl";
	materialDesc.vertexEntryPoint = "SimpleVS";
	materialDesc.pixelEntryPoint = "SimplePS";
	materialDesc.inputLayout = InputLayoutType::PositionTex;
	materialDesc.requiredLayout = VertexLayoutId::PositionTex;

	const Result<MaterialHandle> materialResult =
		resources->GetMaterialServices().CreateMaterial(materialDesc);
	if (!materialResult)
	{
		result.error = materialResult.error.message;
		return result;
	}
	const MaterialHandle materialHandle = materialResult.value;

	MakeIdentity(result.sceneConstants.mvp);
	result.materialConstants.tint[0] = 1.0f;
	result.materialConstants.tint[1] = 1.0f;
	result.materialConstants.tint[2] = 1.0f;
	result.materialConstants.tint[3] = 1.0f;

	ExtractedObject& object = result.extractedObject;
	object.objectId.Index = 0;
	object.objectId.Generation = 1;
	object.mesh = meshHandle;
	object.material = materialHandle;
	object.submeshIndex = 0;
	object.layerMask = RenderLayer::Opaque;
	object.visible = true;
	MakeIdentity(object.worldMatrix);
	FillExtractedObjectOverrides(
		object,
		textureHandle,
		result.sceneConstants,
		result.materialConstants);

	result.success = true;
	return result;
}
