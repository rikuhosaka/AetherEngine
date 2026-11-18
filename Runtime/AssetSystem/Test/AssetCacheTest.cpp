#include "Runtime/AssetSystem/Test/AssetCacheTest.h"

#include "Runtime/AssetSystem/AssetCache.h"
#include "Runtime/AssetSystem/Mesh/CookedMesh.h"
#include "Runtime/AssetSystem/Mesh/PrimitiveMesh.h"
#include "Runtime/Core/Hash/Fnv.h"
#include "Runtime/Core/Log/LogMacros.h"

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

[[nodiscard]] bool SameFloats(const float* left, const float* right, size_t count)
{
	for (size_t index = 0; index < count; ++index)
	{
		if (left[index] != right[index])
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool SameRenderMesh(const PrimitiveMeshData& left, const PrimitiveMeshData& right)
{
	if (left.vertices.size() != right.vertices.size() || left.indices != right.indices
		|| left.bounds.minX != right.bounds.minX || left.bounds.minY != right.bounds.minY
		|| left.bounds.minZ != right.bounds.minZ || left.bounds.maxX != right.bounds.maxX
		|| left.bounds.maxY != right.bounds.maxY || left.bounds.maxZ != right.bounds.maxZ)
	{
		return false;
	}

	for (size_t index = 0; index < left.vertices.size(); ++index)
	{
		const BasicVertex& leftVertex = left.vertices[index];
		const BasicVertex& rightVertex = right.vertices[index];
		if (!SameFloats(leftVertex.position, rightVertex.position, 4)
			|| !SameFloats(leftVertex.uv, rightVertex.uv, 2)
			|| !SameFloats(leftVertex.normal, rightVertex.normal, 3)
			|| !SameFloats(leftVertex.tangent, rightVertex.tangent, 4))
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool CookedFileStartsWithMagic(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	char magic[4] = {};
	file.read(magic, 4);
	return file.gcount() == 4 && magic[0] == 'A' && magic[1] == 'E' && magic[2] == 'M' && magic[3] == 'S';
}

[[nodiscard]] Result<void> TestCookedMeshCache()
{
	const std::filesystem::path root = std::filesystem::temp_directory_path() / "AetherMeshCook";
	std::error_code errorCode{};
	std::filesystem::remove_all(root, errorCode);

	const std::filesystem::path duplicateRoot = root / "duplicate" / "Assets";
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
	if (!firstDuplicate || !secondDuplicate || duplicate || duplicateCache.Find("dup") != nullptr
		|| std::filesystem::exists(CookedMeshPath(duplicateRoot, "a.json"), errorCode)
		|| std::filesystem::exists(CookedMeshPath(duplicateRoot, "b.json"), errorCode))
	{
		std::filesystem::remove_all(root, errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			firstDuplicate && secondDuplicate
				? "Duplicate mesh ids should fail before a cooked mesh is written"
				: (firstDuplicate ? secondDuplicate.error.message : firstDuplicate.error.message));
	}

	const std::filesystem::path triangleRoot = root / "triangle" / "Assets";
	const Result<void> triangleFile = WriteText(
		triangleRoot / "Meshes" / "bad.json",
		R"({"id":"bad:tri","kind":"triangleList","points":[[0,0,0],[1,0,0],[0,1,0]],"indices":[0,1,5]})");
	AssetCache triangleCache{};
	const Result<void> triangle = triangleFile
		? triangleCache.LoadDirectory(triangleRoot)
		: MakeFail(triangleFile.error.code, triangleFile.error.message);
	if (!triangleFile || triangle || triangleCache.Find("bad:tri") != nullptr
		|| std::filesystem::exists(CookedMeshPath(triangleRoot, "bad.json"), errorCode))
	{
		std::filesystem::remove_all(root, errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			triangleFile
				? "Invalid mesh JSON should fail before a cooked mesh is written"
				: triangleFile.error.message);
	}

	const std::filesystem::path cookRoot = root / "cook" / "Assets";
	constexpr std::string_view boxJson = R"({"id":"cook:box","kind":"box","width":1,"height":1,"depth":1})";
	const Result<void> boxFile = WriteText(cookRoot / "Meshes" / "box.json", boxJson);
	AssetCache firstCache{};
	const Result<void> firstLoad = boxFile ? firstCache.LoadDirectory(cookRoot) : boxFile;
	const Result<PrimitiveMeshData> unitBox = firstLoad
		? GenerateCubeMesh({ 1.0f, 1.0f, 1.0f, false })
		: MakeFail<PrimitiveMeshData>(firstLoad.error.code, firstLoad.error.message);
	const PrimitiveMeshData* firstRender = firstLoad ? firstCache.FindRender("cook:box") : nullptr;
	const std::filesystem::path cookedPath = CookedMeshPath(cookRoot, "box.json");
	AssetCache secondCache{};
	const Result<void> secondLoad = firstRender != nullptr ? secondCache.LoadDirectory(cookRoot) : firstLoad;
	const PrimitiveMeshData* secondRender = secondLoad ? secondCache.FindRender("cook:box") : nullptr;
	if (!boxFile || !firstLoad || !unitBox || firstRender == nullptr || !secondLoad || secondRender == nullptr
		|| !std::filesystem::exists(cookedPath, errorCode) || !SameRenderMesh(*firstRender, unitBox.value)
		|| !SameRenderMesh(*secondRender, unitBox.value))
	{
		std::filesystem::remove_all(root, errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			boxFile && firstLoad && secondLoad
				? "Cooked mesh should match the generated box and load again from disk"
				: (boxFile ? (firstLoad ? secondLoad.error.message : firstLoad.error.message) : boxFile.error.message));
	}

	CookedMesh patched{};
	const bool readPatched = TryReadCookedMesh(cookedPath, Fnv1a64(boxJson), patched)
		&& !patched.render.vertices.empty();
	if (readPatched)
	{
		patched.render.vertices[0].position[0] += 3.0f;
	}
	const float patchedPosition = readPatched ? patched.render.vertices[0].position[0] : 0.0f;
	const Result<void> writtenPatch = readPatched
		? WriteCookedMesh(cookedPath, Fnv1a64(boxJson), patched)
		: FailRuntime(LogCategory::Asset, ErrorCode::InvalidArgument, "Cooked mesh could not be read back");
	AssetCache patchedCache{};
	const Result<void> patchedLoad = writtenPatch ? patchedCache.LoadDirectory(cookRoot) : writtenPatch;
	const PrimitiveMeshData* patchedRender = patchedLoad ? patchedCache.FindRender("cook:box") : nullptr;
	if (!writtenPatch || !patchedLoad || patchedRender == nullptr || patchedRender->vertices.empty()
		|| patchedRender->vertices[0].position[0] != patchedPosition)
	{
		std::filesystem::remove_all(root, errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			writtenPatch && patchedLoad
				? "A matching cooked mesh should be reused"
				: (writtenPatch ? patchedLoad.error.message : writtenPatch.error.message));
	}

	constexpr std::string_view wideJson = R"({"id":"cook:box","kind":"box","width":4,"height":1,"depth":1})";
	const Result<void> wideFile = WriteText(cookRoot / "Meshes" / "box.json", wideJson);
	AssetCache wideCache{};
	const Result<void> wideLoad = wideFile ? wideCache.LoadDirectory(cookRoot) : wideFile;
	const Result<PrimitiveMeshData> wideBox = wideLoad
		? GenerateCubeMesh({ 4.0f, 1.0f, 1.0f, false })
		: MakeFail<PrimitiveMeshData>(wideLoad.error.code, wideLoad.error.message);
	const PrimitiveMeshData* wideRender = wideLoad ? wideCache.FindRender("cook:box") : nullptr;
	if (!wideFile || !wideLoad || !wideBox || wideRender == nullptr || !SameRenderMesh(*wideRender, wideBox.value)
		|| wideRender->bounds.maxX == unitBox.value.bounds.maxX)
	{
		std::filesystem::remove_all(root, errorCode);
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			wideFile && wideLoad
				? "A changed mesh JSON should cook a new render mesh"
				: (wideFile ? wideLoad.error.message : wideFile.error.message));
	}

	{
		std::ofstream corrupt(cookedPath, std::ios::binary | std::ios::trunc);
		corrupt << "XXXX";
	}
	AssetCache recoveredCache{};
	const Result<void> recoveredLoad = recoveredCache.LoadDirectory(cookRoot);
	const PrimitiveMeshData* recoveredRender = recoveredLoad ? recoveredCache.FindRender("cook:box") : nullptr;
	const bool recoveredMagic = CookedFileStartsWithMagic(cookedPath);
	const bool recoveredMesh = recoveredLoad && recoveredRender != nullptr
		&& SameRenderMesh(*recoveredRender, wideBox.value);
	std::filesystem::remove_all(root, errorCode);
	if (!recoveredLoad || !recoveredMesh || !recoveredMagic)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			recoveredLoad
				? "A corrupt cooked mesh should be rebuilt from the JSON"
				: recoveredLoad.error.message);
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

	if (auto cooked = TestCookedMeshCache(); !cooked)
	{
		return cooked;
	}

	LOG_INFO(LogCategory::Asset, "AssetCacheTests passed");
	return MakeOk();
}
