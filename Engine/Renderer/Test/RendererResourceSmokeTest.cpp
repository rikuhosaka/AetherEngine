#include "Engine/Renderer/Test/RendererResourceSmokeTest.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Mesh/MeshUpload.h"
#include "Engine/Renderer/Model/Loader/ModelLoadTypes.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Scene/RenderConstantsBuild.h"
#include "Engine/Renderer/Scene/RenderConstantsLayout.h"
#include "Engine/Renderer/Scene/RenderSceneDefaults.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/Renderer/Texture/TextureUpload.h"
#include "Engine/RHI/Common/RHIInput.h"

#include <array>
#include <cmath>
#include <cstring>

namespace
{
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
	const RendererResourceSmokeMaterialConstants& materialConstants)
{
	object.overrides.baseColor = textureHandle;
	object.overrides.normal = {};

	object.overrides.parameters.clear();
	MaterialParameterBlock materialBlock{};
	materialBlock.bindingSlot = 0;
	materialBlock.data = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(&materialConstants),
		sizeof(materialConstants));
	object.overrides.parameters.push_back(materialBlock);
}

[[nodiscard]] bool IsNearlyEqual(float left, float right, float epsilon = 0.0001f)
{
	return std::fabs(left - right) <= epsilon;
}
} // namespace

Result<void> RunRendererResourceSmokeLayoutTests()
{
	static_assert(sizeof(FrameConstants) == 512);
	static_assert(sizeof(ObjectConstants) == 256);

	ExtractedView view{};
	ExtractedLighting lighting{};
	InitDefaultExtractedView(view, 1280, 720);
	InitDefaultExtractedLighting(lighting);

	FrameConstants frameConstants{};
	BuildFrameConstants(view, lighting, frameConstants);

	if (!IsNearlyEqual(frameConstants.ambientColor[3], lighting.ambientIntensity))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"FrameConstants ambient intensity mismatch");
	}

	if (!IsNearlyEqual(frameConstants.mainLightColor[3], lighting.mainLightIntensity))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"FrameConstants main light intensity mismatch");
	}

	const float directionLength = std::sqrt(
		(frameConstants.mainLightDirection[0] * frameConstants.mainLightDirection[0]) +
		(frameConstants.mainLightDirection[1] * frameConstants.mainLightDirection[1]) +
		(frameConstants.mainLightDirection[2] * frameConstants.mainLightDirection[2]));
	if (!IsNearlyEqual(directionLength, 1.0f, 0.01f))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"FrameConstants main light direction is not normalized");
	}

	ObjectConstants objectConstants{};
	float identityWorld[16]{};
	MakeIdentity(identityWorld);
	BuildObjectConstants(identityWorld, objectConstants);
	if (!IsNearlyEqual(objectConstants.worldMatrix[0], 1.0f))
	{
		return FailRuntime(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"BuildObjectConstants world matrix copy mismatch");
	}

	LOG_INFO(LogCategory::Renderer, "RendererResourceSmokeLayoutTests passed");
	return MakeOk();
}

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

	const std::array<BasicVertex, 4> vertices = {
		BasicVertex{
			{ -0.5f, 0.5f, 0.0f, 1.0f },
			{ 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f },
		},
		BasicVertex{
			{ 0.5f, 0.5f, 0.0f, 1.0f },
			{ 1.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f },
		},
		BasicVertex{
			{ 0.5f, -0.5f, 0.0f, 1.0f },
			{ 1.0f, 1.0f },
			{ 0.0f, 0.0f, 1.0f },
		},
		BasicVertex{
			{ -0.5f, -0.5f, 0.0f, 1.0f },
			{ 0.0f, 1.0f },
			{ 0.0f, 0.0f, 1.0f },
		},
	};
	const std::array<uint32_t, 6> indices = { 0, 1, 2, 0, 2, 3 };

	MeshUploadDesc meshDesc{};
	meshDesc.vertices = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(vertices.data()),
		vertices.size() * sizeof(BasicVertex));
	meshDesc.indices = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(indices.data()),
		indices.size() * sizeof(uint32_t));
	meshDesc.vertexStride = sizeof(BasicVertex);
	meshDesc.vertexCount = static_cast<uint32_t>(vertices.size());
	meshDesc.indexCount = static_cast<uint32_t>(indices.size());
	meshDesc.indexFormat = IndexFormat::R32_UINT;
	meshDesc.layoutId = VertexLayoutId::Basic;
	meshDesc.DebugName = "SmokeQuad";

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
	materialDesc.vertexShaderPath = shaderRoot / "LitBasicVS.hlsl";
	materialDesc.pixelShaderPath = shaderRoot / "LitBasicPS.hlsl";
	materialDesc.vertexEntryPoint = "LitBasicVS";
	materialDesc.pixelEntryPoint = "LitBasicPS";
	materialDesc.inputLayout = InputLayoutType::Basic;
	materialDesc.requiredLayout = VertexLayoutId::Basic;

	const Result<MaterialHandle> materialResult =
		resources->GetMaterialServices().CreateMaterial(materialDesc);
	if (!materialResult)
	{
		result.error = materialResult.error.message;
		return result;
	}
	const MaterialHandle materialHandle = materialResult.value;

	Material* material = resources->GetMaterialServices().GetMaterial(materialHandle);
	if (material == nullptr || !MaterialUsesPassConstantBuffers(*material))
	{
		result.error = "Smoke test material does not use pass constant buffers";
		return result;
	}

	InitDefaultExtractedView(result.view, 1280, 720);
	InitDefaultExtractedLighting(result.lighting);
	BuildFrameConstants(result.view, result.lighting, result.frameConstants);

	ExtractedObject& object = result.extractedObject;
	object.objectId.Index = 0;
	object.objectId.Generation = 1;
	object.mesh = meshHandle;
	object.material = materialHandle;
	object.submeshIndex = 0;
	object.layerMask = RenderLayer::Opaque;
	object.visible = true;
	MakeIdentity(object.worldMatrix);
	FillExtractedObjectOverrides(object, textureHandle, result.materialConstants);

	result.success = true;
	return result;
}
