#include "Runtime/Renderer/Scene/SceneAssetUpload.h"

#include "Runtime/Engine/AssetSystem/AssetCache.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Renderer/Material/MaterialSystemServices.h"
#include "Runtime/Renderer/Mesh/MeshSystemServices.h"
#include "Runtime/Renderer/Mesh/PrimitiveMesh.h"
#include "Runtime/Renderer/Resource/RenderResourceServices.h"
#include "Runtime/Renderer/Scene/RenderConstantsLayout.h"
#include "Runtime/Renderer/Texture/TextureSystemServices.h"
#include "Runtime/RHI/Interface/RHICommandList.h"
#include "Runtime/Renderer/Scene/WorldRenderBridge.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace
{
[[nodiscard]] bool ContainsKey(const std::vector<std::string>& keys, std::string_view key)
{
	for (const std::string& existing : keys)
	{
		if (existing == key)
		{
			return true;
		}
	}

	return false;
}

[[nodiscard]] Result<PrimitiveMeshData> GenerateAssetMesh(const MeshAsset& asset)
{
	switch (asset.kind)
	{
	case MeshAssetKind::Box:
		return GenerateCubeMesh({ asset.width, asset.height, asset.depth, false });
	case MeshAssetKind::Sphere:
		return GenerateSphereMesh({ asset.radius, asset.stacks, asset.slices });
	case MeshAssetKind::Capsule:
		return GenerateCapsuleMesh({
			asset.radius,
			asset.halfHeight,
			asset.slices,
			asset.hemisphereStacks });
	case MeshAssetKind::ConvexHull:
		return GenerateConvexHullMesh({ asset.points });
	case MeshAssetKind::TriangleList:
		return GenerateTriangleMesh({ asset.points, asset.indices });
	default:
		return FailInternal<PrimitiveMeshData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Scene asset mesh kind is unsupported");
	}
}

[[nodiscard]] Result<MeshHandle> UploadPrimitive(
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const PrimitiveMeshData& mesh,
	const char* debugName)
{
	return resources.GetMeshServices().UploadMesh(
		MakePrimitiveMeshUploadDesc(mesh, debugName),
		frameContext,
		commandList);
}

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
	const SceneDocument& document,
	const AssetCache& cache,
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& shaderRoot)
{
	if (commandList == nullptr)
	{
		return FailInternal<UploadedSceneAssets>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"UploadSceneAssets requires a valid command list");
	}

	if (shaderRoot.empty() || assetsRoot.empty())
	{
		return FailInternal<UploadedSceneAssets>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"UploadSceneAssets requires valid shader and assets roots");
	}

	std::vector<std::string> keys{};
	for (const SceneEntity& entity : document.entities)
	{
		if (!entity.model.empty())
		{
			continue;
		}

		if (entity.mesh.empty())
		{
			return FailInternal<UploadedSceneAssets>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene asset mesh key is empty");
		}

		if (!ContainsKey(keys, entity.mesh))
		{
			keys.push_back(entity.mesh);
		}
	}

	UploadedSceneAssets assets{};
	assets.meshes.reserve(keys.size());
	for (const std::string& key : keys)
	{
		const MeshAsset* asset = cache.Find(key);
		if (asset == nullptr)
		{
			return FailRuntime<UploadedSceneAssets>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene asset mesh was not found: " + key);
		}

		const Result<PrimitiveMeshData> mesh = GenerateAssetMesh(*asset);
		if (!mesh)
		{
			return MakeFail<UploadedSceneAssets>(mesh.error.code, mesh.error.message);
		}

		const Result<MeshHandle> uploaded = UploadPrimitive(
			resources, frameContext, commandList, mesh.value, key.c_str());
		if (!uploaded)
		{
			return MakeFail<UploadedSceneAssets>(uploaded.error.code, uploaded.error.message);
		}

		assets.meshes.push_back(SceneMeshRef{ key, ToWorldMeshId(uploaded.value) });
	}

	for (const SceneEntity& entity : document.entities)
	{
		if (entity.model.empty())
		{
			continue;
		}

		auto model = std::make_unique<FbxSceneAssets>();
		const Result<void> loaded = model->EnsureInitialized(
			resources,
			frameContext,
			commandList,
			shaderRoot,
			assetsRoot,
			entity.model);
		if (!loaded)
		{
			return MakeFail<UploadedSceneAssets>(loaded.error.code, loaded.error.message);
		}

		assets.models.push_back(SceneModelAsset{ entity.id, std::move(model) });
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

	assets.render.material = ToWorldMaterialId(material.value);
	assets.render.baseColor = ToWorldTextureId(texture.value);
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
