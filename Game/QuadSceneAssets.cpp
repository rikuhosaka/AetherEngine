#include "Game/QuadSceneAssets.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Core/Renderer.h"
#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Mesh/MeshUpload.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Texture/Loader/TextureLoadTypes.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/RHI/Common/RHIInput.h"
#include "Engine/RHI/Interface/RHICommandList.h"

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
} // namespace

Result<void> QuadSceneAssets::EnsureInitialized(
	Renderer& renderer,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot,
	const QuadMaterialConstants& color)
{
	if (m_ready)
	{
		return MakeOk();
	}

	if (commandList == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"QuadSceneAssets requires a valid command list");
	}

	if (shaderRoot.empty())
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"QuadSceneAssets requires a valid shader root path");
	}

	RenderResourceServices* resources = renderer.GetResourceServices();
	if (resources == nullptr)
	{
		return FailInternal(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"QuadSceneAssets requires renderer resource services");
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
	meshDesc.DebugName = "ScreenQuad";

	const Result<MeshHandle> meshResult =
		resources->GetMeshServices().UploadMesh(meshDesc, frameContext, commandList);
	if (!meshResult)
	{
		return MakeFail(meshResult.error.code, meshResult.error.message);
	}
	m_mesh = meshResult.value;

	TextureLoadDesc textureLoadDesc{};
	textureLoadDesc.relativePath = "Textures/GameIcon.png";
	textureLoadDesc.colorSpace = TextureColorSpace::Srgb;
	textureLoadDesc.generateMips = true;
	textureLoadDesc.debugName = "GameIcon";

	const Result<TextureHandle> textureResult =
		resources->GetTextureServices().GetOrLoadTexture(
			textureLoadDesc,
			frameContext,
			commandList);
	if (!textureResult)
	{
		return MakeFail(textureResult.error.code, textureResult.error.message);
	}
	m_baseColorTexture = textureResult.value;

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
		return MakeFail(materialResult.error.code, materialResult.error.message);
	}
	m_material = materialResult.value;

	MakeIdentity(m_sceneConstants.mvp);
	m_materialConstants = color;

	m_ready = true;
	LOG_INFO(LogCategory::Core, "QuadSceneAssets initialized (screen quad with GameIcon texture)");
	return MakeOk();
}

void QuadSceneAssets::FillExtractedObject(ExtractedObject& object) const
{
	object.mesh = m_mesh;
	object.material = m_material;
	object.submeshIndex = 0;
	object.overrides.baseColor = m_baseColorTexture;
	object.overrides.normal = {};

	object.overrides.parameters.clear();
	object.overrides.parameters.reserve(2);

	MaterialParameterBlock sceneBlock{};
	sceneBlock.bindingSlot = 0;
	sceneBlock.data = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(&m_sceneConstants),
		sizeof(m_sceneConstants));
	object.overrides.parameters.push_back(sceneBlock);

	MaterialParameterBlock materialBlock{};
	materialBlock.bindingSlot = 1;
	materialBlock.data = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(&m_materialConstants),
		sizeof(m_materialConstants));
	object.overrides.parameters.push_back(materialBlock);
}
