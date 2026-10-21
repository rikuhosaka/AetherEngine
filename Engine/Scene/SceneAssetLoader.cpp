#include "Engine/Scene/SceneAssetLoader.h"

#include "Engine/Scene/FbxSceneAssets.h"

#include "Engine/AssetSystem/AssetCache.h"
#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Mesh/PrimitiveMesh.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Scene/RenderConstantsLayout.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/Scene/WorldRenderBridge.h"

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

Result<void> ApplyMeshCollision(SceneDocument& document, const AssetCache& cache)
{
	for (SceneEntity& entity : document.entities)
	{
		if (!entity.body.has_value())
		{
			continue;
		}

		RigidBodyShape& shape = entity.body->shape;
		if (shape.type != PhysicsShapeType::Mesh && shape.type != PhysicsShapeType::ConvexHull)
		{
			continue;
		}

		if (shape.mesh.empty())
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene mesh collision asset is empty");
		}

		const MeshAsset* asset = cache.Find(shape.mesh);
		if (asset == nullptr)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene mesh collision asset was not found: " + shape.mesh);
		}

		const bool triangle = shape.type == PhysicsShapeType::Mesh && asset->kind == MeshAssetKind::TriangleList;
		const bool hull = shape.type == PhysicsShapeType::ConvexHull && asset->kind == MeshAssetKind::ConvexHull;
		if (!triangle && !hull)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene mesh collision asset kind does not match the shape");
		}

		shape.points = asset->points;
		shape.indices = asset->indices;
	}

	return MakeOk();
}

Result<SceneAssets> SceneAssetLoader::Load(
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
		return FailInternal<SceneAssets>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"SceneAssetLoader requires a valid command list");
	}

	if (shaderRoot.empty() || assetsRoot.empty())
	{
		return FailInternal<SceneAssets>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"SceneAssetLoader requires valid shader and assets roots");
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
			return FailInternal<SceneAssets>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene asset mesh key is empty");
		}

		if (!ContainsKey(keys, entity.mesh))
		{
			keys.push_back(entity.mesh);
		}
	}

	SceneAssets assets{};
	assets.meshes.reserve(keys.size());
	for (const std::string& key : keys)
	{
		const MeshAsset* asset = cache.Find(key);
		if (asset == nullptr)
		{
			return FailRuntime<SceneAssets>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene asset mesh was not found: " + key);
		}

		const Result<PrimitiveMeshData> mesh = GenerateAssetMesh(*asset);
		if (!mesh)
		{
			return MakeFail<SceneAssets>(mesh.error.code, mesh.error.message);
		}

		const Result<MeshHandle> uploaded = UploadPrimitive(
			resources, frameContext, commandList, mesh.value, key.c_str());
		if (!uploaded)
		{
			return MakeFail<SceneAssets>(uploaded.error.code, uploaded.error.message);
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
			return MakeFail<SceneAssets>(loaded.error.code, loaded.error.message);
		}

		assets.models.push_back(SceneModelAsset{ entity.id, std::move(model) });
	}

	const Result<TextureHandle> texture = UploadWhiteTexture(resources, frameContext, commandList);
	if (!texture)
	{
		return MakeFail<SceneAssets>(texture.error.code, texture.error.message);
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
		return MakeFail<SceneAssets>(material.error.code, material.error.message);
	}

	const Material* createdMaterial = resources.GetMaterialServices().GetMaterial(material.value);
	if (createdMaterial == nullptr)
	{
		return FailInternal<SceneAssets>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"SceneAssetLoader failed to resolve created material");
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
