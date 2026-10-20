#include "Engine/AssetSystem/AssetCache.h"

#include "Engine/Core/Log/LogMacros.h"
#include "ThirdParty/nlohmann/json.hpp"

#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace
{
using Json = nlohmann::json;

[[nodiscard]] std::filesystem::path CanonicalizeIfPossible(const std::filesystem::path& path)
{
	std::error_code errorCode{};
	const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, errorCode);
	return errorCode ? path : canonicalPath;
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

[[nodiscard]] bool IsPathUnderRoot(
	const std::filesystem::path& candidate,
	const std::filesystem::path& root)
{
	std::error_code errorCode{};
	const std::filesystem::path relative = std::filesystem::relative(candidate, root, errorCode);
	if (errorCode || relative.empty())
	{
		return candidate == root;
	}

	return !ContainsParentReference(relative);
}

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

[[nodiscard]] const Json* FindField(const Json& object, const char* key)
{
	if (!object.is_object())
	{
		return nullptr;
	}

	const Json::const_iterator field = object.find(key);
	if (field == object.end())
	{
		return nullptr;
	}

	return &(*field);
}

[[nodiscard]] bool ReadUint32(const Json& value, uint32_t& out)
{
	if (!value.is_number_unsigned())
	{
		return false;
	}

	const uint64_t number = value.get<uint64_t>();
	if (number > UINT32_MAX)
	{
		return false;
	}

	out = static_cast<uint32_t>(number);
	return true;
}

[[nodiscard]] bool ReadFloat(const Json& value, float& out)
{
	if (!value.is_number())
	{
		return false;
	}

	out = value.get<float>();
	return std::isfinite(out);
}

[[nodiscard]] bool ReadFloats(const Json& value, float* out, size_t count)
{
	if (!value.is_array() || value.size() != count)
	{
		return false;
	}

	for (size_t index = 0; index < count; ++index)
	{
		if (!ReadFloat(value[index], out[index]))
		{
			return false;
		}
	}

	return true;
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
		if (!ReadFloats(pointValue, &point.x, 3))
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
		if (!ReadUint32(indexValue, index) || index >= pointCount)
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

	const Json* id = FindField(json, "id");
	const Json* kindValue = FindField(json, "kind");
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
		const Json* points = FindField(json, "points");
		const Json* indices = FindField(json, "indices");
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
		const Json* points = FindField(json, "points");
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
		const Json* width = FindField(json, "width");
		const Json* height = FindField(json, "height");
		const Json* depth = FindField(json, "depth");
		if (width == nullptr || height == nullptr || depth == nullptr || !ReadFloat(*width, asset.width)
			|| !ReadFloat(*height, asset.height) || !ReadFloat(*depth, asset.depth) || asset.width <= 0.0f
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
		const Json* radius = FindField(json, "radius");
		const Json* stacks = FindField(json, "stacks");
		const Json* slices = FindField(json, "slices");
		if (radius == nullptr || stacks == nullptr || slices == nullptr || !ReadFloat(*radius, asset.radius)
			|| !ReadUint32(*stacks, asset.stacks) || !ReadUint32(*slices, asset.slices) || asset.radius <= 0.0f
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
		const Json* radius = FindField(json, "radius");
		const Json* halfHeight = FindField(json, "halfHeight");
		const Json* slices = FindField(json, "slices");
		const Json* hemisphereStacks = FindField(json, "hemisphereStacks");
		if (radius == nullptr || halfHeight == nullptr || slices == nullptr || hemisphereStacks == nullptr
			|| !ReadFloat(*radius, asset.radius) || !ReadFloat(*halfHeight, asset.halfHeight)
			|| !ReadUint32(*slices, asset.slices) || !ReadUint32(*hemisphereStacks, asset.hemisphereStacks)
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

[[nodiscard]] const MeshAsset* FindAsset(const std::vector<MeshAsset>& assets, std::string_view id)
{
	for (const MeshAsset& asset : assets)
	{
		if (asset.id == id)
		{
			return &asset;
		}
	}

	return nullptr;
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

	std::vector<MeshAsset> loaded{};
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

		const std::filesystem::path relativePath = std::filesystem::path("Meshes") / iterator->path().filename();
		Result<MeshAsset> asset = LoadMeshAssetFile(assetsRoot, relativePath);
		if (!asset)
		{
			return MakeFail(asset.error.code, asset.error.message);
		}

		if (FindAsset(loaded, asset.value.id) != nullptr)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Mesh asset id is already loaded: " + asset.value.id);
		}

		loaded.push_back(std::move(asset.value));
	}

	m_assets = std::move(loaded);
	m_loaded = true;
	return MakeOk();
}

const MeshAsset* AssetCache::Find(std::string_view id) const
{
	return FindAsset(m_assets, id);
}
