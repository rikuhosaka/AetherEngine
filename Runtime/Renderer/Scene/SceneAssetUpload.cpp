#include "Runtime/Renderer/Scene/SceneAssetUpload.h"

#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Renderer/Material/MaterialSystemServices.h"
#include "Runtime/Renderer/Mesh/MeshSystemServices.h"
#include "Runtime/Renderer/Resource/RenderResourceServices.h"
#include "Runtime/Renderer/Scene/RenderConstantsLayout.h"
#include "Runtime/Renderer/Texture/TextureSystemServices.h"
#include "Runtime/RHI/Interface/RHICommandList.h"

#include <array>
#include <utility>

namespace
{
[[nodiscard]] Result<TextureHandle> UploadWhiteTexture(
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
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

	return resources.GetTextureServices().UploadTexture(textureDesc, frameContext, commandList);
}
} // namespace

Result<UploadedSceneAssets> UploadSceneAssets(
	const SceneGpuUpload& upload,
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot)
{
	if (commandList == nullptr)
	{
		return FailInternal<UploadedSceneAssets>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"UploadSceneAssets requires a valid command list");
	}

	if (shaderRoot.empty())
	{
		return FailInternal<UploadedSceneAssets>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"UploadSceneAssets requires a valid shader root");
	}

	UploadedSceneAssets assets{};
	assets.meshes.reserve(upload.meshes.size());
	for (const SceneMeshUpload& mesh : upload.meshes)
	{
		const Result<MeshHandle> uploaded = resources.GetMeshServices().UploadMesh(
			mesh.mesh,
			frameContext,
			commandList);
		if (!uploaded)
		{
			return MakeFail<UploadedSceneAssets>(uploaded.error.code, uploaded.error.message);
		}

		assets.meshes.push_back(UploadedMeshRef{ mesh.key, uploaded.value });
	}

	std::vector<TextureHandle> textures{};
	textures.reserve(upload.textures.size());
	for (const SceneTextureUpload& texture : upload.textures)
	{
		const Result<TextureHandle> uploaded = resources.GetTextureServices().UploadTexture(
			texture.image,
			frameContext,
			commandList,
			texture.cacheKey);
		if (!uploaded)
		{
			return MakeFail<UploadedSceneAssets>(uploaded.error.code, uploaded.error.message);
		}

		textures.push_back(uploaded.value);
	}

	for (const SceneModelUpload& model : upload.models)
	{
		auto assetsModel = std::make_unique<FbxSceneAssets>();
		const Result<void> loaded = assetsModel->EnsureInitialized(
			resources,
			frameContext,
			commandList,
			shaderRoot,
			model,
			textures);
		if (!loaded)
		{
			return MakeFail<UploadedSceneAssets>(loaded.error.code, loaded.error.message);
		}

		assets.models.push_back(SceneModelAsset{ model.documentId, std::move(assetsModel) });
	}

	const Result<TextureHandle> texture = UploadWhiteTexture(resources, frameContext, commandList);
	if (!texture)
	{
		return MakeFail<UploadedSceneAssets>(texture.error.code, texture.error.message);
	}

	MaterialCreateDesc materialDesc{};
	materialDesc.vertexShaderPath = shaderRoot / "LitBasicVS.hlsl";
	materialDesc.pixelShaderPath = shaderRoot / "LitBasicPS.hlsl";
	materialDesc.vertexEntryPoint = "LitBasicVS";
	materialDesc.pixelEntryPoint = "LitBasicPS";
	materialDesc.inputLayout = InputLayoutType::Basic;
	materialDesc.requiredLayout = VertexLayoutId::Basic;

	const Result<MaterialHandle> material = resources.GetMaterialServices().CreateMaterial(materialDesc);
	if (!material)
	{
		return MakeFail<UploadedSceneAssets>(material.error.code, material.error.message);
	}

	const Material* createdMaterial = resources.GetMaterialServices().GetMaterial(material.value);
	if (createdMaterial == nullptr)
	{
		return FailInternal<UploadedSceneAssets>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"UploadSceneAssets failed to resolve created material");
	}

	assets.render.material = material.value;
	assets.render.baseColor = texture.value;
	for (uint32_t layoutIndex = 0; layoutIndex < createdMaterial->constantLayout.size(); ++layoutIndex)
	{
		const ShaderConstantBuffer& layout = createdMaterial->constantLayout[layoutIndex];
		if (layout.Register == RenderRegisters::MaterialConstants && layout.Space == 0)
		{
			assets.render.materialConstantsSlot = layoutIndex;
			break;
		}
	}

	return MakeOk(std::move(assets));
}
