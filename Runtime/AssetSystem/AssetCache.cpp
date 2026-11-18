#include "Runtime/AssetSystem/AssetCache.h"

#include "Runtime/AssetSystem/Mesh/PrimitiveMesh.h"
#include "Runtime/Core/Hash/Fnv.h"
#include "Runtime/Core/IO/BinaryFile.h"
#include "Runtime/Core/IO/Path.h"
#include "Runtime/Core/Json/JsonFields.h"
#include "Runtime/Core/Log/LogMacros.h"
#include "ThirdParty/nlohmann/json.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace
{
using Json = nlohmann::json;

[[nodiscard]] bool HasJsonExtension(const std::filesystem::path& path)
{
	std::string extension = path.extension().string();
	for (char& character : extension)
	{
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	return extension == ".json";
}

[[nodiscard]] bool IsMeshesRelativePath(const std::filesystem::path& path)
{
	const std::filesystem::path::iterator begin = path.begin();
	if (begin == path.end() || *begin != "Meshes")
	{
		return false;
	}

	std::filesystem::path::iterator next = begin;
	++next;
	return next != path.end();
}

[[nodiscard]] Result<std::vector<DirectX::XMFLOAT3>> ReadPoints(const Json& value)
{
	if (!value.is_array() || value.empty())
	{
		return FailRuntime<std::vector<DirectX::XMFLOAT3>>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset is missing points");
	}

	std::vector<DirectX::XMFLOAT3> points{};
	points.reserve(value.size());
	for (const Json& pointValue : value)
	{
		DirectX::XMFLOAT3 point{};
		if (!JsonFields::ReadFloats(pointValue, &point.x, 3))
		{
			return FailRuntime<std::vector<DirectX::XMFLOAT3>>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Mesh asset point must have three finite components");
		}

		points.push_back(point);
	}

	return MakeOk(std::move(points));
}

[[nodiscard]] Result<std::vector<uint32_t>> ReadIndices(const Json& value, size_t pointCount)
{
	if (!value.is_array() || value.empty() || value.size() % 3 != 0)
	{
		return FailRuntime<std::vector<uint32_t>>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset triangle indices must be a non-empty multiple of three");
	}

	std::vector<uint32_t> indices{};
	indices.reserve(value.size());
	for (const Json& indexValue : value)
	{
		uint32_t index = 0;
		if (!JsonFields::ReadUint32(indexValue, index) || index >= pointCount)
		{
			return FailRuntime<std::vector<uint32_t>>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Mesh asset triangle index is out of range");
		}

		indices.push_back(index);
	}

	return MakeOk(std::move(indices));
}

[[nodiscard]] bool ReadKind(const Json& value, MeshAssetKind& out)
{
	if (!value.is_string())
	{
		return false;
	}

	const std::string kind = value.get<std::string>();
	if (kind == "triangleList")
	{
		out = MeshAssetKind::TriangleList;
		return true;
	}
	if (kind == "convexHull")
	{
		out = MeshAssetKind::ConvexHull;
		return true;
	}
	if (kind == "box")
	{
		out = MeshAssetKind::Box;
		return true;
	}
	if (kind == "sphere")
	{
		out = MeshAssetKind::Sphere;
		return true;
	}
	if (kind == "capsule")
	{
		out = MeshAssetKind::Capsule;
		return true;
	}

	return false;
}

[[nodiscard]] Result<MeshAsset> ReadMeshAsset(std::string_view text)
{
	Json json{};
	try
	{
		json = Json::parse(text);
	}
	catch (const Json::exception&)
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset JSON is invalid");
	}

	if (!json.is_object())
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset must be an object");
	}

	const Json* id = JsonFields::FindField(json, "id");
	const Json* kindValue = JsonFields::FindField(json, "kind");
	if (id == nullptr || !id->is_string() || id->get<std::string>().empty())
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset id is missing");
	}

	MeshAsset asset{};
	asset.id = id->get<std::string>();
	if (kindValue == nullptr || !kindValue->is_string())
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset kind is missing");
	}

	if (!ReadKind(*kindValue, asset.kind))
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset kind is unsupported");
	}

	switch (asset.kind)
	{
	case MeshAssetKind::TriangleList:
	{
		const Json* points = JsonFields::FindField(json, "points");
		const Json* indices = JsonFields::FindField(json, "indices");
		if (points == nullptr || indices == nullptr)
		{
			return FailRuntime<MeshAsset>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Mesh asset triangle list is missing points or indices");
		}

		Result<std::vector<DirectX::XMFLOAT3>> readPoints = ReadPoints(*points);
		if (!readPoints)
		{
			return MakeFail<MeshAsset>(readPoints.error.code, readPoints.error.message);
		}

		Result<std::vector<uint32_t>> readIndices = ReadIndices(*indices, readPoints.value.size());
		if (!readIndices)
		{
			return MakeFail<MeshAsset>(readIndices.error.code, readIndices.error.message);
		}

		asset.points = std::move(readPoints.value);
		asset.indices = std::move(readIndices.value);
		break;
	}
	case MeshAssetKind::ConvexHull:
	{
		const Json* points = JsonFields::FindField(json, "points");
		if (points == nullptr)
		{
			return FailRuntime<MeshAsset>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Mesh asset convex hull is missing points");
		}

		Result<std::vector<DirectX::XMFLOAT3>> readPoints = ReadPoints(*points);
		if (!readPoints)
		{
			return MakeFail<MeshAsset>(readPoints.error.code, readPoints.error.message);
		}

		if (readPoints.value.size() < 4)
		{
			return FailRuntime<MeshAsset>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Mesh asset convex hull requires at least four points");
		}

		asset.points = std::move(readPoints.value);
		break;
	}
	case MeshAssetKind::Box:
	{
		const Json* width = JsonFields::FindField(json, "width");
		const Json* height = JsonFields::FindField(json, "height");
		const Json* depth = JsonFields::FindField(json, "depth");
		if (width == nullptr || height == nullptr || depth == nullptr || !JsonFields::ReadFloat(*width, asset.width)
			|| !JsonFields::ReadFloat(*height, asset.height) || !JsonFields::ReadFloat(*depth, asset.depth) || asset.width <= 0.0f
			|| asset.height <= 0.0f || asset.depth <= 0.0f)
		{
			return FailRuntime<MeshAsset>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Mesh asset box requires positive width, height, and depth");
		}
		break;
	}
	case MeshAssetKind::Sphere:
	{
		const Json* radius = JsonFields::FindField(json, "radius");
		const Json* stacks = JsonFields::FindField(json, "stacks");
		const Json* slices = JsonFields::FindField(json, "slices");
		if (radius == nullptr || stacks == nullptr || slices == nullptr || !JsonFields::ReadFloat(*radius, asset.radius)
			|| !JsonFields::ReadUint32(*stacks, asset.stacks) || !JsonFields::ReadUint32(*slices, asset.slices) || asset.radius <= 0.0f
			|| asset.stacks < 2 || asset.slices < 3)
		{
			return FailRuntime<MeshAsset>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Mesh asset sphere requires a positive radius, at least 2 stacks, and at least 3 slices");
		}
		break;
	}
	case MeshAssetKind::Capsule:
	{
		const Json* radius = JsonFields::FindField(json, "radius");
		const Json* halfHeight = JsonFields::FindField(json, "halfHeight");
		const Json* slices = JsonFields::FindField(json, "slices");
		const Json* hemisphereStacks = JsonFields::FindField(json, "hemisphereStacks");
		if (radius == nullptr || halfHeight == nullptr || slices == nullptr || hemisphereStacks == nullptr
			|| !JsonFields::ReadFloat(*radius, asset.radius) || !JsonFields::ReadFloat(*halfHeight, asset.halfHeight)
			|| !JsonFields::ReadUint32(*slices, asset.slices) || !JsonFields::ReadUint32(*hemisphereStacks, asset.hemisphereStacks)
			|| asset.radius <= 0.0f || asset.halfHeight <= 0.0f || asset.slices < 3 || asset.hemisphereStacks < 1)
		{
			return FailRuntime<MeshAsset>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Mesh asset capsule requires a positive radius and half height");
		}
		break;
	}
	default:
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset kind is unsupported");
	}

	return MakeOk(std::move(asset));
}

struct PendingMesh
{
	std::filesystem::path fileName{};
	uint64_t sourceHash = 0;
	MeshAsset source{};
};

[[nodiscard]] bool ContainsId(const std::vector<PendingMesh>& pending, std::string_view id)
{
	for (const PendingMesh& mesh : pending)
	{
		if (mesh.source.id == id)
		{
			return true;
		}
	}

	return false;
}

[[nodiscard]] const CookedMesh* FindCooked(const std::vector<CookedMesh>& meshes, std::string_view id)
{
	for (const CookedMesh& mesh : meshes)
	{
		if (mesh.source.id == id)
		{
			return &mesh;
		}
	}

	return nullptr;
}

[[nodiscard]] Result<PrimitiveMeshData> GenerateMeshRender(const MeshAsset& asset)
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
		return FailRuntime<PrimitiveMeshData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset kind is unsupported");
	}
}
} // namespace

Result<MeshAsset> LoadMeshAssetFile(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& relativePath)
{
	if (assetsRoot.empty())
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Assets root path is empty");
	}

	if (relativePath.empty())
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset path is empty");
	}

	if (relativePath.is_absolute())
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset path must be relative to Assets/: " + relativePath.string());
	}

	if (ContainsParentReference(relativePath))
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset path must not traverse outside Assets/: " + relativePath.string());
	}

	if (!IsMeshesRelativePath(relativePath) || !HasJsonExtension(relativePath))
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset path must be a .json file under Assets/Meshes/: " + relativePath.string());
	}

	const std::filesystem::path canonicalRoot = CanonicalizeIfPossible(assetsRoot);
	const std::filesystem::path absolutePath = CanonicalizeIfPossible(canonicalRoot / relativePath);
	const std::filesystem::path meshesRoot = CanonicalizeIfPossible(canonicalRoot / "Meshes");
	if (!IsPathUnderRoot(absolutePath, meshesRoot))
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset path must stay under Assets/Meshes/: " + relativePath.string());
	}

	std::error_code errorCode{};
	if (!std::filesystem::exists(absolutePath, errorCode) || errorCode)
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Mesh asset file not found: " + absolutePath.string());
	}

	std::ifstream file(absolutePath);
	if (!file)
	{
		return FailRuntime<MeshAsset>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Mesh asset file could not be opened");
	}

	const std::string text{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	return ReadMeshAsset(text);
}

Result<void> AssetCache::LoadDirectory(const std::filesystem::path& assetsRoot)
{
	if (m_loaded)
	{
		return MakeOk();
	}

	if (assetsRoot.empty())
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Assets root path is empty");
	}

	const std::filesystem::path meshesRoot = assetsRoot / "Meshes";
	std::error_code errorCode{};
	if (!std::filesystem::is_directory(meshesRoot, errorCode) || errorCode)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Mesh asset directory was not found");
	}

	std::vector<PendingMesh> pending{};
	std::filesystem::directory_iterator iterator(meshesRoot, errorCode);
	const std::filesystem::directory_iterator end{};
	if (errorCode)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Mesh asset directory could not be opened");
	}

	for (; iterator != end; iterator.increment(errorCode))
	{
		if (errorCode)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::FileNotFound,
				"Mesh asset directory could not be read");
		}

		if (!iterator->is_regular_file(errorCode) || errorCode || !HasJsonExtension(iterator->path()))
		{
			errorCode.clear();
			continue;
		}

		Result<std::string> bytes = ReadBinaryFile(iterator->path());
		if (!bytes)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::FileNotFound,
				"Mesh asset file could not be opened");
		}

		Result<MeshAsset> asset = ReadMeshAsset(bytes.value);
		if (!asset)
		{
			return MakeFail(asset.error.code, asset.error.message);
		}

		if (ContainsId(pending, asset.value.id))
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Mesh asset id is already loaded: " + asset.value.id);
		}

		PendingMesh item{};
		item.fileName = iterator->path().filename();
		item.sourceHash = Fnv1a64(bytes.value);
		item.source = std::move(asset.value);
		pending.push_back(std::move(item));
	}

	std::vector<CookedMesh> loaded{};
	loaded.reserve(pending.size());
	for (PendingMesh& item : pending)
	{
		const std::filesystem::path cookedPath = CookedMeshPath(assetsRoot, item.fileName);
		CookedMesh cooked{};
		const bool reusable = TryReadCookedMesh(cookedPath, item.sourceHash, cooked)
			&& cooked.source.id == item.source.id && cooked.source.kind == item.source.kind;
		if (!reusable)
		{
			Result<PrimitiveMeshData> render = GenerateMeshRender(item.source);
			if (!render)
			{
				return MakeFail(render.error.code, render.error.message);
			}

			cooked.source = std::move(item.source);
			cooked.render = std::move(render.value);
			Result<void> written = WriteCookedMesh(cookedPath, item.sourceHash, cooked);
			if (!written)
			{
				return MakeFail(written.error.code, written.error.message);
			}
		}

		loaded.push_back(std::move(cooked));
	}

	m_meshes = std::move(loaded);
	m_loaded = true;
	return MakeOk();
}

const MeshAsset* AssetCache::Find(std::string_view id) const
{
	const CookedMesh* mesh = FindCooked(m_meshes, id);
	return mesh == nullptr ? nullptr : &mesh->source;
}

const PrimitiveMeshData* AssetCache::FindRender(std::string_view id) const
{
	const CookedMesh* mesh = FindCooked(m_meshes, id);
	return mesh == nullptr ? nullptr : &mesh->render;
}
