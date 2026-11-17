#include "Runtime/AssetSystem/Model/Loader/CookedModel.h"

#include "Runtime/Core/IO/BinaryCursor.h"
#include "Runtime/AssetSystem/Model/Loader/FbxModelFileLoader.h"
#include "Runtime/AssetSystem/Model/Loader/FbxSdkContext.h"
#include "Runtime/AssetSystem/Model/Loader/ModelAssetPath.h"
#include "Runtime/Core/Log/LogMacros.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

namespace
{
constexpr uint32_t kMaxCookedCount = 1000000;
constexpr uint32_t kMaxStringBytes = 65536;
constexpr char kMagic[4] = { 'A', 'E', 'M', 'O' };
constexpr uint8_t kPathEmpty = 0;
constexpr uint8_t kPathAssetsRelative = 1;
constexpr uint8_t kPathAbsolute = 2;

[[nodiscard]] std::filesystem::path AssetRoot(const std::filesystem::path& assetsRoot)
{
	std::filesystem::path root = assetsRoot.lexically_normal();
	if (root.filename().empty())
	{
		root = root.parent_path();
	}

	std::error_code errorCode{};
	const std::filesystem::path canonical = std::filesystem::weakly_canonical(root, errorCode);
	return errorCode ? root : canonical;
}

[[nodiscard]] std::string ToUtf8(const std::filesystem::path& path)
{
	const std::u8string text = path.generic_u8string();
	return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

[[nodiscard]] std::filesystem::path FromUtf8(std::string_view text)
{
	const std::u8string bytes(reinterpret_cast<const char8_t*>(text.data()), text.size());
	return std::filesystem::path(bytes);
}

[[nodiscard]] std::filesystem::path CanonicalizeIfPossible(const std::filesystem::path& path)
{
	std::error_code errorCode{};
	const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, errorCode);
	return errorCode ? path : canonical;
}

[[nodiscard]] bool ContainsParentReference(const std::filesystem::path& path)
{
	for (const std::filesystem::path& part : path)
	{
		if (part == "..")
		{
			return true;
		}
	}

	return false;
}

[[nodiscard]] bool FitsCount(size_t count)
{
	return count <= kMaxCookedCount;
}

void WriteVertex(BinaryWriter& writer, const BasicVertex& vertex)
{
	for (float value : vertex.position)
	{
		writer.F32(value);
	}
	for (float value : vertex.uv)
	{
		writer.F32(value);
	}
	for (float value : vertex.normal)
	{
		writer.F32(value);
	}
	for (float value : vertex.tangent)
	{
		writer.F32(value);
	}
}

[[nodiscard]] BasicVertex ReadVertex(BinaryReader& reader)
{
	BasicVertex vertex{};
	for (float& value : vertex.position)
	{
		value = reader.F32();
	}
	for (float& value : vertex.uv)
	{
		value = reader.F32();
	}
	for (float& value : vertex.normal)
	{
		value = reader.F32();
	}
	for (float& value : vertex.tangent)
	{
		value = reader.F32();
	}
	return vertex;
}

void WriteStoredPath(
	BinaryWriter& writer,
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& path)
{
	if (path.empty())
	{
		writer.U8(kPathEmpty);
		writer.String({});
		return;
	}

	std::error_code errorCode{};
	const std::filesystem::path relative = std::filesystem::relative(path, assetsRoot, errorCode);
	if (!errorCode && !relative.empty() && !ContainsParentReference(relative))
	{
		writer.U8(kPathAssetsRelative);
		writer.String(ToUtf8(relative));
		return;
	}

	writer.U8(kPathAbsolute);
	writer.String(ToUtf8(path));
}

[[nodiscard]] std::filesystem::path ReadStoredPath(BinaryReader& reader, const std::filesystem::path& assetsRoot)
{
	const uint8_t kind = reader.U8();
	const std::string text = reader.String(kMaxStringBytes);
	if (!reader.Reading())
	{
		return {};
	}

	if (kind == kPathEmpty)
	{
		return {};
	}

	const std::filesystem::path parsed = FromUtf8(text);
	if (parsed.empty() || ContainsParentReference(parsed))
	{
		reader.Fail();
		return {};
	}

	if (kind == kPathAssetsRelative)
	{
		return CanonicalizeIfPossible(assetsRoot / parsed);
	}

	if (kind == kPathAbsolute)
	{
		return CanonicalizeIfPossible(parsed);
	}

	reader.Fail();
	return {};
}

void WriteModel(
	BinaryWriter& writer,
	uint64_t sourceHash,
	const FbxModelLoadOptions& options,
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	const ModelAssetData& model)
{
	writer.Bytes(kMagic, 4);
	writer.U32(kCookedModelVersion);
	writer.U64(sourceHash);
	writer.U8(options.triangulate ? 1 : 0);
	writer.U8(options.generateNormalsIfMissing ? 1 : 0);
	writer.U8(options.convertAxisToDirectX ? 1 : 0);
	writer.U8(options.convertUnitsToMeters ? 1 : 0);
	writer.String(ToUtf8(relativePath.lexically_normal()));

	writer.U32(model.rootNodeIndex);
	writer.U32(static_cast<uint32_t>(model.meshes.size()));
	for (const ModelMeshData& mesh : model.meshes)
	{
		writer.String(mesh.name);
		writer.U32(static_cast<uint32_t>(mesh.vertices.size()));
		for (const BasicVertex& vertex : mesh.vertices)
		{
			WriteVertex(writer, vertex);
		}

		writer.U32(static_cast<uint32_t>(mesh.indices.size()));
		for (uint32_t index : mesh.indices)
		{
			writer.U32(index);
		}

		writer.U32(static_cast<uint32_t>(mesh.submeshes.size()));
		for (const ModelSubMeshData& submesh : mesh.submeshes)
		{
			writer.U32(submesh.indexStart);
			writer.U32(submesh.indexCount);
			writer.U32(submesh.materialSlot);
		}

		writer.F32(mesh.bounds.minX);
		writer.F32(mesh.bounds.minY);
		writer.F32(mesh.bounds.minZ);
		writer.F32(mesh.bounds.maxX);
		writer.F32(mesh.bounds.maxY);
		writer.F32(mesh.bounds.maxZ);
	}

	writer.U32(static_cast<uint32_t>(model.materialSlots.size()));
	for (const ModelMaterialSlotData& slot : model.materialSlots)
	{
		writer.String(slot.name);
		WriteStoredPath(writer, assetsRoot, slot.diffuseTexturePath);
		WriteStoredPath(writer, assetsRoot, slot.normalTexturePath);
	}

	writer.U32(static_cast<uint32_t>(model.nodes.size()));
	for (const ModelNodeData& node : model.nodes)
	{
		writer.String(node.name);
		for (float value : node.localTransform)
		{
			writer.F32(value);
		}
		writer.I32(node.parentIndex);
		writer.I32(node.meshIndex);
		writer.U32(static_cast<uint32_t>(node.childNodeIndices.size()));
		for (uint32_t child : node.childNodeIndices)
		{
			writer.U32(child);
		}
	}
}

[[nodiscard]] bool OptionsMatch(BinaryReader& reader, const FbxModelLoadOptions& options)
{
	const bool triangulate = reader.U8() != 0;
	const bool generateNormals = reader.U8() != 0;
	const bool convertAxis = reader.U8() != 0;
	const bool convertUnits = reader.U8() != 0;
	return reader.Reading() && triangulate == options.triangulate && generateNormals == options.generateNormalsIfMissing
		&& convertAxis == options.convertAxisToDirectX && convertUnits == options.convertUnitsToMeters;
}

[[nodiscard]] bool ReadModel(
	BinaryReader& reader,
	uint64_t sourceHash,
	const FbxModelLoadOptions& options,
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	ModelAssetData& outModel)
{
	char magic[4] = {};
	for (char& character : magic)
	{
		character = static_cast<char>(reader.U8());
	}
	if (!reader.Reading() || std::memcmp(magic, kMagic, 4) != 0 || reader.U32() != kCookedModelVersion
		|| reader.U64() != sourceHash || !OptionsMatch(reader, options))
	{
		return false;
	}

	const std::string storedRelative = reader.String(kMaxStringBytes);
	if (!reader.Reading() || storedRelative != ToUtf8(relativePath.lexically_normal()))
	{
		return false;
	}

	ModelAssetData model{};
	model.rootNodeIndex = reader.U32();
	const uint32_t meshCount = reader.Count(kMaxCookedCount);
	if (!reader.Reading() || meshCount == 0)
	{
		return false;
	}

	model.meshes.reserve(meshCount);
	for (uint32_t meshIndex = 0; meshIndex < meshCount; ++meshIndex)
	{
		ModelMeshData mesh{};
		mesh.name = reader.String(kMaxStringBytes);
		const uint32_t vertexCount = reader.Count(kMaxCookedCount);
		if (!reader.Reading() || vertexCount == 0)
		{
			return false;
		}

		mesh.vertices.reserve(vertexCount);
		for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
		{
			mesh.vertices.push_back(ReadVertex(reader));
			if (!reader.Reading())
			{
				return false;
			}
		}

		const uint32_t indexCount = reader.Count(kMaxCookedCount);
		if (!reader.Reading() || indexCount == 0)
		{
			return false;
		}
		mesh.indices.resize(indexCount);
		for (uint32_t& index : mesh.indices)
		{
			index = reader.U32();
		}

		const uint32_t submeshCount = reader.Count(kMaxCookedCount);
		if (!reader.Reading())
		{
			return false;
		}
		mesh.submeshes.resize(submeshCount);
		for (ModelSubMeshData& submesh : mesh.submeshes)
		{
			submesh.indexStart = reader.U32();
			submesh.indexCount = reader.U32();
			submesh.materialSlot = reader.U32();
		}

		mesh.bounds.minX = reader.F32();
		mesh.bounds.minY = reader.F32();
		mesh.bounds.minZ = reader.F32();
		mesh.bounds.maxX = reader.F32();
		mesh.bounds.maxY = reader.F32();
		mesh.bounds.maxZ = reader.F32();
		if (!reader.Reading())
		{
			return false;
		}

		model.meshes.push_back(std::move(mesh));
	}

	const uint32_t slotCount = reader.Count(kMaxCookedCount);
	if (!reader.Reading())
	{
		return false;
	}
	model.materialSlots.reserve(slotCount);
	for (uint32_t slotIndex = 0; slotIndex < slotCount; ++slotIndex)
	{
		ModelMaterialSlotData slot{};
		slot.name = reader.String(kMaxStringBytes);
		slot.diffuseTexturePath = ReadStoredPath(reader, assetsRoot);
		slot.normalTexturePath = ReadStoredPath(reader, assetsRoot);
		if (!reader.Reading())
		{
			return false;
		}
		model.materialSlots.push_back(std::move(slot));
	}

	const uint32_t nodeCount = reader.Count(kMaxCookedCount);
	if (!reader.Reading() || nodeCount == 0 || model.rootNodeIndex >= nodeCount)
	{
		return false;
	}

	model.nodes.reserve(nodeCount);
	for (uint32_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
	{
		ModelNodeData node{};
		node.name = reader.String(kMaxStringBytes);
		for (float& value : node.localTransform)
		{
			value = reader.F32();
		}
		node.parentIndex = reader.I32();
		node.meshIndex = reader.I32();
		const uint32_t childCount = reader.Count(kMaxCookedCount);
		if (!reader.Reading())
		{
			return false;
		}
		node.childNodeIndices.resize(childCount);
		for (uint32_t& child : node.childNodeIndices)
		{
			child = reader.U32();
		}
		if (!reader.Reading() || node.parentIndex < -1 || node.meshIndex < -1
			|| (node.meshIndex >= 0 && static_cast<uint32_t>(node.meshIndex) >= model.meshes.size()))
		{
			return false;
		}
		model.nodes.push_back(std::move(node));
	}

	if (!reader.Exhausted())
	{
		return false;
	}

	for (const ModelNodeData& node : model.nodes)
	{
		if (node.parentIndex >= static_cast<int32_t>(model.nodes.size()))
		{
			return false;
		}

		for (uint32_t child : node.childNodeIndices)
		{
			if (child >= model.nodes.size())
			{
				return false;
			}
		}
	}

	outModel = std::move(model);
	return true;
}

[[nodiscard]] bool ModelFits(const ModelAssetData& model)
{
	if (!FitsCount(model.meshes.size()) || !FitsCount(model.materialSlots.size()) || !FitsCount(model.nodes.size())
		|| model.meshes.empty())
	{
		return false;
	}

	for (const ModelMeshData& mesh : model.meshes)
	{
		if (!FitsCount(mesh.vertices.size()) || !FitsCount(mesh.indices.size()) || !FitsCount(mesh.submeshes.size())
			|| mesh.vertices.empty() || mesh.indices.empty() || mesh.name.size() > kMaxStringBytes)
		{
			return false;
		}
	}

	for (const ModelMaterialSlotData& slot : model.materialSlots)
	{
		if (slot.name.size() > kMaxStringBytes)
		{
			return false;
		}
	}

	for (const ModelNodeData& node : model.nodes)
	{
		if (!FitsCount(node.childNodeIndices.size()) || node.name.size() > kMaxStringBytes)
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] Result<std::string> ReadFileBytes(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return FailRuntime<std::string>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Model file could not be opened");
	}

	return MakeOk(std::string{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() });
}
} // namespace

std::filesystem::path CookedModelPath(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath)
{
	std::filesystem::path cookedRelative = relativePath.lexically_normal();
	cookedRelative.replace_extension(".aemodel");
	return AssetRoot(assetsRoot).parent_path() / "Intermediate" / "Cooked" / "Models" / cookedRelative;
}

uint64_t HashModelSource(std::string_view bytes)
{
	uint64_t hash = 14695981039346656037ull;
	for (const char character : bytes)
	{
		hash ^= static_cast<uint8_t>(character);
		hash *= 1099511628211ull;
	}

	return hash;
}

bool TryReadCookedModel(
	const std::filesystem::path& path,
	uint64_t sourceHash,
	const FbxModelLoadOptions& options,
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	ModelAssetData& outModel)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return false;
	}

	const std::vector<uint8_t> bytes{
		std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>() };
	if (bytes.empty())
	{
		return false;
	}

	BinaryReader reader(bytes.data(), bytes.size());
	return ReadModel(reader, sourceHash, options, AssetRoot(assetsRoot), relativePath, outModel);
}

Result<void> WriteCookedModel(
	const std::filesystem::path& path,
	uint64_t sourceHash,
	const FbxModelLoadOptions& options,
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	const ModelAssetData& model)
{
	if (relativePath.empty() || ContainsParentReference(relativePath) || !ModelFits(model))
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Cooked model is missing a relative path or a render mesh");
	}

	std::error_code errorCode{};
	std::filesystem::create_directories(path.parent_path(), errorCode);
	if (errorCode)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Cooked model directory could not be created");
	}

	BinaryWriter writer{};
	WriteModel(writer, sourceHash, options, AssetRoot(assetsRoot), relativePath, model);

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Cooked model file could not be opened");
	}

	file.write(
		reinterpret_cast<const char*>(writer.Data().data()),
		static_cast<std::streamsize>(writer.Data().size()));
	if (!file)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Cooked model file could not be written");
	}

	return MakeOk();
}

Result<ModelAssetData> LoadModelAsset(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath,
	const FbxModelLoadOptions& options)
{
	const Result<std::filesystem::path> resolved = ResolveModelAssetPath(assetsRoot, relativePath);
	if (!resolved)
	{
		return MakeFail<ModelAssetData>(resolved.error.code, resolved.error.message);
	}

	const Result<std::string> bytes = ReadFileBytes(resolved.value);
	if (!bytes)
	{
		return MakeFail<ModelAssetData>(bytes.error.code, bytes.error.message);
	}

	const uint64_t sourceHash = HashModelSource(bytes.value);
	const std::filesystem::path cookedPath = CookedModelPath(assetsRoot, relativePath);
	ModelAssetData cooked{};
	if (TryReadCookedModel(cookedPath, sourceHash, options, assetsRoot, relativePath, cooked))
	{
		cooked.sourcePath = resolved.value;
		return MakeOk(std::move(cooked));
	}

	Result<FbxSdkContext> sdk = FbxSdkContext::Create();
	if (!sdk)
	{
		return MakeFail<ModelAssetData>(sdk.error.code, sdk.error.message);
	}

	Result<ModelAssetData> loaded = LoadModelAssetDataFromFile(resolved.value, sdk.value, options);
	if (!loaded)
	{
		return MakeFail<ModelAssetData>(loaded.error.code, loaded.error.message);
	}

	const Result<void> written = WriteCookedModel(
		cookedPath,
		sourceHash,
		options,
		assetsRoot,
		relativePath,
		loaded.value);
	if (!written)
	{
		return MakeFail<ModelAssetData>(written.error.code, written.error.message);
	}

	return loaded;
}
