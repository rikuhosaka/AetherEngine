#include "Engine/AssetSystem/Test/AssetCacheTest.h"

#include "Engine/AssetSystem/AssetCache.h"
#include "Engine/Core/Log/LogMacros.h"

#include <fstream>
#include <string_view>
#include <system_error>

namespace
{
[[nodiscard]] Result<void> WriteText(const std::filesystem::path& path, std::string_view text)
{
	std::error_code errorCode{};
	std::filesystem::create_directories(path.parent_path(), errorCode);
	if (errorCode)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Mesh asset test directory could not be created");
	}

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Mesh asset test file could not be opened");
	}

	file << text;
	if (!file)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Mesh asset test file could not be written");
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestCachedAssets(const std::filesystem::path& assetsRoot)
{
	AssetCache cache{};
	const Result<void> loaded = cache.LoadDirectory(assetsRoot);
	const MeshAsset* first = loaded ? cache.Find("builtin:floor") : nullptr;
	const MeshAsset* second = cache.Find("builtin:floor");
	const Result<void> loadedAgain = loaded ? cache.LoadDirectory(assetsRoot) : loaded;
	const MeshAsset* third = cache.Find("builtin:floor");
	const MeshAsset* box = cache.Find("builtin:box");
	const Result<MeshAsset> direct = LoadMeshAssetFile(assetsRoot, "Meshes/floor.json");
	if (!loaded || !loadedAgain || first == nullptr || first != second || second != third
		|| first->kind != MeshAssetKind::TriangleList || first->points.size() != 4 || first->indices.size() != 6
		|| first->indices[0] != 0 || first->indices[5] != 1 || first->points[0].x != -4.0f
		|| cache.Find("builtin:missing") != nullptr || box == nullptr || box->kind != MeshAssetKind::Box
		|| box->width != 4.0f || box->height != 1.0f || box->depth != 4.0f || !direct
		|| direct.value.id != first->id || direct.value.indices.size() != first->indices.size())
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			loaded && loadedAgain
				? "Mesh asset cache should keep one copy of each id"
				: (loaded ? loadedAgain.error.message : loaded.error.message));
	}

	return MakeOk();
}

[[nodiscard]] Result<void> TestRejectedAssets(const std::filesystem::path& assetsRoot)
{
	const Result<MeshAsset> absolute = LoadMeshAssetFile(assetsRoot, assetsRoot / "Meshes" / "floor.json");
	const Result<MeshAsset> parent = LoadMeshAssetFile(assetsRoot, "Meshes/../Scenes/PhysicsDrop.json");
	const Result<MeshAsset> scene = LoadMeshAssetFile(assetsRoot, "Scenes/PhysicsDrop.json");
	if (absolute || parent || scene)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset paths outside Assets/Meshes should be rejected");
	}

	const std::filesystem::path root = std::filesystem::temp_directory_path() / "AetherMeshAssetCache";
	std::error_code errorCode{};
	std::filesystem::remove_all(root, errorCode);

	const std::filesystem::path duplicateRoot = root / "duplicate";
	const Result<void> firstDuplicate = WriteText(
		duplicateRoot / "Meshes" / "a.json",
		R"({"id":"dup","kind":"box","width":1,"height":1,"depth":1})");
	const Result<void> secondDuplicate = firstDuplicate
		? WriteText(
			duplicateRoot / "Meshes" / "b.json",
			R"({"id":"dup","kind":"box","width":1,"height":1,"depth":1})")
		: firstDuplicate;
	AssetCache duplicateCache{};
	const Result<void> duplicate = secondDuplicate
		? duplicateCache.LoadDirectory(duplicateRoot)
		: MakeFail(secondDuplicate.error.code, secondDuplicate.error.message);
	if (!firstDuplicate || !secondDuplicate || duplicate || duplicateCache.Find("dup") != nullptr)
	{
		std::filesystem::remove_all(root, errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			firstDuplicate && secondDuplicate
				? "Duplicate mesh asset ids should fail to load"
				: (firstDuplicate ? secondDuplicate.error.message : firstDuplicate.error.message));
	}

	const std::filesystem::path triangleRoot = root / "triangle";
	const Result<void> triangleFile = WriteText(
		triangleRoot / "Meshes" / "bad.json",
		R"({"id":"bad:tri","kind":"triangleList","points":[[0,0,0],[1,0,0],[0,1,0]],"indices":[0,1,5]})");
	AssetCache triangleCache{};
	const Result<void> triangle = triangleFile
		? triangleCache.LoadDirectory(triangleRoot)
		: MakeFail(triangleFile.error.code, triangleFile.error.message);
	if (!triangleFile || triangle || triangleCache.Find("bad:tri") != nullptr)
	{
		std::filesystem::remove_all(root, errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			triangleFile
				? "Out of range mesh triangles should fail to load"
				: triangleFile.error.message);
	}

	const std::filesystem::path reuseRoot = root / "reuse";
	const Result<void> keptFile = WriteText(
		reuseRoot / "Meshes" / "kept.json",
		R"({"id":"kept","kind":"box","width":1,"height":1,"depth":1})");
	AssetCache reuseCache{};
	const Result<void> kept = keptFile ? reuseCache.LoadDirectory(reuseRoot) : keptFile;
	const MeshAsset* keptAsset = kept ? reuseCache.Find("kept") : nullptr;
	const Result<void> extraFile = kept
		? WriteText(
			reuseRoot / "Meshes" / "extra.json",
			R"({"id":"extra","kind":"box","width":1,"height":1,"depth":1})")
		: kept;
	const Result<void> keptAgain = extraFile ? reuseCache.LoadDirectory(reuseRoot) : extraFile;
	std::filesystem::remove_all(root, errorCode);
	if (!kept || keptAsset == nullptr || !extraFile || !keptAgain || reuseCache.Find("kept") != keptAsset
		|| reuseCache.Find("extra") != nullptr)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Mesh asset cache should not read the same directory again");
	}

	return MakeOk();
}
} // namespace

Result<void> RunAssetCacheTests(const std::filesystem::path& assetsRoot)
{
	if (auto cached = TestCachedAssets(assetsRoot); !cached)
	{
		return cached;
	}

	if (auto rejected = TestRejectedAssets(assetsRoot); !rejected)
	{
		return rejected;
	}

	LOG_INFO(LogCategory::Asset, "AssetCacheTests passed");
	return MakeOk();
}
