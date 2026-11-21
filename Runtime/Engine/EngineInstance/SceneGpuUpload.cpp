#include "Runtime/Engine/EngineInstance/SceneGpuUpload.h"

#include "Runtime/AssetSystem/AssetCache.h"
#include "Runtime/AssetSystem/Model/Loader/CookedModel.h"
#include "Runtime/AssetSystem/Texture/Loader/CookedTexture.h"
#include "Runtime/AssetSystem/Texture/Loader/TextureFileLoader.h"
#include "Runtime/Core/IO/Path.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Renderer/Mesh/PrimitiveMesh.h"
#include "Runtime/Renderer/Texture/Loader/TextureImageToUpload.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace
{
constexpr uint32_t kWhiteTextureIndex = 0;
constexpr const char* kWhiteTextureKey = "builtin:white";

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

[[nodiscard]] TextureImageData MakeWhiteImage()
{
	TextureImageData image{};
	image.width = 1;
	image.height = 1;
	image.mipCount = 1;
	image.format = TextureImageFormat::R8G8B8A8_UNORM;
	TextureImageMip mip{};
	mip.width = 1;
	mip.height = 1;
	mip.rowPitch = 4;
	mip.pixels = {
		std::byte{ 255 },
		std::byte{ 255 },
		std::byte{ 255 },
		std::byte{ 255 },
	};
	image.mips.push_back(std::move(mip));
	return image;
}

[[nodiscard]] std::filesystem::path MakeAssetsRelativePath(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& texturePath)
{
	if (!texturePath.is_absolute())
	{
		return texturePath;
	}

	std::error_code errorCode{};
	const std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(assetsRoot, errorCode);
	if (errorCode)
	{
		return {};
	}

	const std::filesystem::path relativePath =
		std::filesystem::relative(texturePath, canonicalRoot, errorCode);
	if (errorCode || relativePath.empty() || ContainsParentReference(relativePath))
	{
		return {};
	}

	return relativePath;
}

[[nodiscard]] std::string MakeTextureCacheKey(
	const std::filesystem::path& relativePath,
	TextureColorSpace colorSpace)
{
	std::string key = relativePath.generic_string();
	key += colorSpace == TextureColorSpace::Srgb ? "|srgb" : "|linear";
	key += "|mips";
	return key;
}

[[nodiscard]] MeshUploadDesc MakeModelMeshUploadDesc(const ModelMeshData& meshData)
{
	std::vector<SubmeshRange> submeshRanges;
	submeshRanges.reserve(meshData.submeshes.size());
	for (const ModelSubMeshData& submesh : meshData.submeshes)
	{
		SubmeshRange range{};
		range.indexStart = submesh.indexStart;
		range.indexCount = submesh.indexCount;
		range.materialSlot = submesh.materialSlot;
		submeshRanges.push_back(range);
	}

	MeshUploadDesc meshDesc{};
	meshDesc.vertices = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(meshData.vertices.data()),
		meshData.vertices.size() * sizeof(BasicVertex));
	meshDesc.indices = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(meshData.indices.data()),
		meshData.indices.size() * sizeof(uint32_t));
	meshDesc.vertexStride = sizeof(BasicVertex);
	meshDesc.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
	meshDesc.indexCount = static_cast<uint32_t>(meshData.indices.size());
	meshDesc.indexFormat = IndexFormat::R32_UINT;
	meshDesc.layoutId = VertexLayoutId::Basic;
	meshDesc.submeshes = std::move(submeshRanges);
	meshDesc.bounds = meshData.bounds;
	return meshDesc;
}

void BindUploadNames(SceneGpuUpload& upload)
{
	for (SceneMeshUpload& mesh : upload.meshes)
	{
		mesh.mesh.DebugName = mesh.key.c_str();
	}

	for (SceneTextureUpload& texture : upload.textures)
	{
		texture.image.DebugName = texture.debugName.c_str();
	}

	for (SceneModelUpload& model : upload.models)
	{
		for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
		{
			model.meshes[meshIndex].DebugName = model.meshNames[meshIndex].c_str();
		}
	}
}

[[nodiscard]] Result<uint32_t> ResolveModelTexture(
	SceneGpuUploadSources& sources,
	std::unordered_map<std::string, uint32_t>& loaded,
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& texturePath,
	TextureColorSpace colorSpace,
	const char* debugName)
{
	const std::filesystem::path relativePath = MakeAssetsRelativePath(assetsRoot, texturePath);
	if (relativePath.empty())
	{
		if (!texturePath.empty())
		{
			LOG_WARN_F(LogCategory::Asset,
				"Model texture is outside Assets/, using white fallback: {}", texturePath.string());
		}

		return MakeOk(kWhiteTextureIndex);
	}

	const std::string cacheKey = MakeTextureCacheKey(relativePath, colorSpace);
	if (const auto found = loaded.find(cacheKey); found != loaded.end())
	{
		return MakeOk(found->second);
	}

	TextureFileLoadOptions options{};
	options.colorSpace = colorSpace;
	options.generateMips = true;
	const Result<TextureImageData> image = LoadTextureImage(assetsRoot, relativePath, options);
	if (!image)
	{
		LOG_WARN_F(LogCategory::Asset,
			"Model texture load failed for {}: {}",
			relativePath.string(),
			image.error.message);
		return MakeOk(kWhiteTextureIndex);
	}

	const uint32_t index = static_cast<uint32_t>(sources.textures.size());
	sources.textures.push_back(std::move(image.value));
	sources.textureCacheKeys.push_back(cacheKey);
	sources.textureDebugNames.push_back(debugName);
	loaded.emplace(cacheKey, index);
	return MakeOk(index);
}
} // namespace

Result<void> BuildSceneGpuUpload(
	const SceneDocument& document,
	const AssetCache& cache,
	const std::filesystem::path& assetsRoot,
	SceneGpuUploadSources& out)
{
	out = {};
	if (assetsRoot.empty())
	{
		return FailInternal(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"BuildSceneGpuUpload requires an assets root");
	}

	SceneGpuUploadSources& sources = out;
	std::vector<std::string> meshKeys{};
	std::vector<uint32_t> modelDocumentIds{};
	for (const SceneEntity& entity : document.entities)
	{
		if (!entity.model.empty())
		{
			continue;
		}

		if (entity.mesh.empty())
		{
			return FailInternal(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene asset mesh key is empty");
		}

		if (!ContainsKey(meshKeys, entity.mesh))
		{
			meshKeys.push_back(entity.mesh);
		}
	}

	sources.primitives.reserve(meshKeys.size());
	for (const std::string& key : meshKeys)
	{
		const PrimitiveMeshData* mesh = cache.FindRender(key);
		if (mesh == nullptr)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene asset mesh was not found: " + key);
		}

		sources.primitives.push_back(*mesh);
	}

	for (const SceneEntity& entity : document.entities)
	{
		if (entity.model.empty())
		{
			continue;
		}

		const Result<ModelAssetData> loaded = LoadModelAsset(assetsRoot, entity.model);
		if (!loaded)
		{
			return MakeFail(loaded.error.code, loaded.error.message);
		}

		sources.models.push_back(std::move(loaded.value));
		modelDocumentIds.push_back(entity.id);
	}

	sources.textures.push_back(MakeWhiteImage());
	sources.textureCacheKeys.emplace_back(kWhiteTextureKey);
	sources.textureDebugNames.emplace_back("White");

	std::unordered_map<std::string, uint32_t> loadedTextures{};
	std::vector<std::vector<uint32_t>> baseColorByModel{};
	std::vector<std::vector<uint32_t>> normalByModel{};
	baseColorByModel.reserve(sources.models.size());
	normalByModel.reserve(sources.models.size());
	for (const ModelAssetData& model : sources.models)
	{
		std::vector<uint32_t> baseColors{};
		std::vector<uint32_t> normals{};
		baseColors.reserve(model.materialSlots.size());
		normals.reserve(model.materialSlots.size());
		for (const ModelMaterialSlotData& slot : model.materialSlots)
		{
			const Result<uint32_t> diffuse = ResolveModelTexture(
				sources,
				loadedTextures,
				assetsRoot,
				slot.diffuseTexturePath,
				TextureColorSpace::Srgb,
				"FbxBaseColor");
			if (!diffuse)
			{
				return MakeFail(diffuse.error.code, diffuse.error.message);
			}

			baseColors.push_back(diffuse.value);
			if (slot.normalTexturePath.empty())
			{
				normals.push_back(kNoSceneTexture);
				continue;
			}

			const Result<uint32_t> normal = ResolveModelTexture(
				sources,
				loadedTextures,
				assetsRoot,
				slot.normalTexturePath,
				TextureColorSpace::Linear,
				"FbxNormal");
			if (!normal)
			{
				return MakeFail(normal.error.code, normal.error.message);
			}

			normals.push_back(normal.value);
		}

		baseColorByModel.push_back(std::move(baseColors));
		normalByModel.push_back(std::move(normals));
	}

	sources.upload.meshes.reserve(sources.primitives.size());
	for (std::size_t meshIndex = 0; meshIndex < sources.primitives.size(); ++meshIndex)
	{
		SceneMeshUpload mesh{};
		mesh.key = meshKeys[meshIndex];
		mesh.mesh = MakePrimitiveMeshUploadDesc(sources.primitives[meshIndex], nullptr);
		sources.upload.meshes.push_back(std::move(mesh));
	}

	sources.upload.textures.reserve(sources.textures.size());
	for (std::size_t textureIndex = 0; textureIndex < sources.textures.size(); ++textureIndex)
	{
		SceneTextureUpload texture{};
		texture.cacheKey = sources.textureCacheKeys[textureIndex];
		texture.debugName = sources.textureDebugNames[textureIndex];
		texture.image = BuildTextureUploadDesc(sources.textures[textureIndex], nullptr);
		sources.upload.textures.push_back(std::move(texture));
	}

	sources.upload.models.reserve(sources.models.size());
	for (std::size_t modelIndex = 0; modelIndex < sources.models.size(); ++modelIndex)
	{
		const ModelAssetData& asset = sources.models[modelIndex];
		SceneModelUpload model{};
		model.documentId = modelDocumentIds[modelIndex];
		model.meshNames.reserve(asset.meshes.size());
		model.meshes.reserve(asset.meshes.size());
		for (const ModelMeshData& meshData : asset.meshes)
		{
			model.meshNames.push_back(meshData.name);
			model.meshes.push_back(MakeModelMeshUploadDesc(meshData));
		}

		model.nodes.reserve(asset.nodes.size());
		for (const ModelNodeData& node : asset.nodes)
		{
			SceneNodeUpload record{};
			record.name = node.name;
			std::memcpy(record.localTransform, node.localTransform, sizeof(record.localTransform));
			record.parentIndex = node.parentIndex;
			record.meshIndex = node.meshIndex;
			model.nodes.push_back(std::move(record));
		}

		model.baseColorBySlot = std::move(baseColorByModel[modelIndex]);
		model.normalBySlot = std::move(normalByModel[modelIndex]);
		sources.upload.models.push_back(std::move(model));
	}

	BindUploadNames(sources.upload);
	return MakeOk();
}
