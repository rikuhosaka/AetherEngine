#include "Runtime/AssetSystem/Mesh/CookedMesh.h"

#include "Runtime/Core/IO/BinaryCursor.h"
#include "Runtime/Core/Log/LogMacros.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace
{
constexpr uint32_t kMaxCookedCount = 1000000;
constexpr uint32_t kMaxIdBytes = 4096;
constexpr char kMagic[4] = { 'A', 'E', 'M', 'S' };

[[nodiscard]] bool IsMeshKind(uint8_t kind)
{
	switch (static_cast<MeshAssetKind>(kind))
	{
	case MeshAssetKind::TriangleList:
	case MeshAssetKind::ConvexHull:
	case MeshAssetKind::Box:
	case MeshAssetKind::Sphere:
	case MeshAssetKind::Capsule:
		return true;
	default:
		return false;
	}
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

void WriteMesh(BinaryWriter& writer, uint64_t sourceHash, const CookedMesh& mesh)
{
	writer.Bytes(kMagic, 4);
	writer.U32(kCookedMeshVersion);
	writer.U64(sourceHash);
	writer.String(mesh.source.id);
	writer.U8(static_cast<uint8_t>(mesh.source.kind));

	writer.U32(static_cast<uint32_t>(mesh.source.points.size()));
	for (const DirectX::XMFLOAT3& point : mesh.source.points)
	{
		writer.F32(point.x);
		writer.F32(point.y);
		writer.F32(point.z);
	}

	writer.U32(static_cast<uint32_t>(mesh.source.indices.size()));
	for (uint32_t index : mesh.source.indices)
	{
		writer.U32(index);
	}

	writer.F32(mesh.source.width);
	writer.F32(mesh.source.height);
	writer.F32(mesh.source.depth);
	writer.F32(mesh.source.radius);
	writer.F32(mesh.source.halfHeight);
	writer.U32(mesh.source.stacks);
	writer.U32(mesh.source.slices);
	writer.U32(mesh.source.hemisphereStacks);

	writer.U32(static_cast<uint32_t>(mesh.render.vertices.size()));
	for (const BasicVertex& vertex : mesh.render.vertices)
	{
		WriteVertex(writer, vertex);
	}

	writer.U32(static_cast<uint32_t>(mesh.render.indices.size()));
	for (uint32_t index : mesh.render.indices)
	{
		writer.U32(index);
	}

	writer.F32(mesh.render.bounds.minX);
	writer.F32(mesh.render.bounds.minY);
	writer.F32(mesh.render.bounds.minZ);
	writer.F32(mesh.render.bounds.maxX);
	writer.F32(mesh.render.bounds.maxY);
	writer.F32(mesh.render.bounds.maxZ);
}

[[nodiscard]] bool ReadMesh(BinaryReader& reader, uint64_t sourceHash, CookedMesh& outMesh)
{
	char magic[4] = {};
	for (char& character : magic)
	{
		character = static_cast<char>(reader.U8());
	}
	if (!reader.Reading() || std::memcmp(magic, kMagic, 4) != 0 || reader.U32() != kCookedMeshVersion
		|| reader.U64() != sourceHash)
	{
		return false;
	}

	CookedMesh mesh{};
	mesh.source.id = reader.String(kMaxIdBytes);
	if (reader.Reading() && mesh.source.id.empty())
	{
		reader.Fail();
	}
	const uint8_t kind = reader.U8();
	if (!reader.Reading() || !IsMeshKind(kind))
	{
		return false;
	}
	mesh.source.kind = static_cast<MeshAssetKind>(kind);

	const uint32_t pointCount = reader.Count(kMaxCookedCount);
	if (!reader.Reading())
	{
		return false;
	}
	mesh.source.points.resize(pointCount);
	for (DirectX::XMFLOAT3& point : mesh.source.points)
	{
		point.x = reader.F32();
		point.y = reader.F32();
		point.z = reader.F32();
		if (!reader.Reading())
		{
			return false;
		}
	}

	const uint32_t sourceIndexCount = reader.Count(kMaxCookedCount);
	if (!reader.Reading())
	{
		return false;
	}
	mesh.source.indices.resize(sourceIndexCount);
	for (uint32_t& index : mesh.source.indices)
	{
		index = reader.U32();
	}

	mesh.source.width = reader.F32();
	mesh.source.height = reader.F32();
	mesh.source.depth = reader.F32();
	mesh.source.radius = reader.F32();
	mesh.source.halfHeight = reader.F32();
	mesh.source.stacks = reader.U32();
	mesh.source.slices = reader.U32();
	mesh.source.hemisphereStacks = reader.U32();

	const uint32_t vertexCount = reader.Count(kMaxCookedCount);
	if (!reader.Reading() || vertexCount == 0)
	{
		return false;
	}
	mesh.render.vertices.reserve(vertexCount);
	for (uint32_t index = 0; index < vertexCount; ++index)
	{
		mesh.render.vertices.push_back(ReadVertex(reader));
		if (!reader.Reading())
		{
			return false;
		}
	}

	const uint32_t renderIndexCount = reader.Count(kMaxCookedCount);
	if (!reader.Reading() || renderIndexCount == 0)
	{
		return false;
	}
	mesh.render.indices.resize(renderIndexCount);
	for (uint32_t& index : mesh.render.indices)
	{
		index = reader.U32();
	}

	mesh.render.bounds.minX = reader.F32();
	mesh.render.bounds.minY = reader.F32();
	mesh.render.bounds.minZ = reader.F32();
	mesh.render.bounds.maxX = reader.F32();
	mesh.render.bounds.maxY = reader.F32();
	mesh.render.bounds.maxZ = reader.F32();
	if (!reader.Exhausted())
	{
		return false;
	}

	outMesh = std::move(mesh);
	return true;
}
} // namespace

std::filesystem::path CookedMeshPath(
	const std::filesystem::path& assetsRoot,
	const std::filesystem::path& jsonFileName)
{
	std::filesystem::path root = assetsRoot.lexically_normal();
	if (root.filename().empty())
	{
		root = root.parent_path();
	}

	std::filesystem::path fileName = jsonFileName.filename();
	fileName.replace_extension(".aemesh");
	return root.parent_path() / "Intermediate" / "Cooked" / "Meshes" / fileName;
}

uint64_t HashMeshSource(std::string_view bytes)
{
	uint64_t hash = 14695981039346656037ull;
	for (const char character : bytes)
	{
		hash ^= static_cast<uint8_t>(character);
		hash *= 1099511628211ull;
	}

	return hash;
}

bool TryReadCookedMesh(
	const std::filesystem::path& path,
	uint64_t sourceHash,
	CookedMesh& outMesh)
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
	return ReadMesh(reader, sourceHash, outMesh);
}

Result<void> WriteCookedMesh(
	const std::filesystem::path& path,
	uint64_t sourceHash,
	const CookedMesh& mesh)
{
	if (mesh.source.id.empty() || mesh.source.id.size() > kMaxIdBytes || mesh.render.vertices.empty()
		|| mesh.render.indices.empty() || mesh.source.points.size() > kMaxCookedCount
		|| mesh.source.indices.size() > kMaxCookedCount || mesh.render.vertices.size() > kMaxCookedCount
		|| mesh.render.indices.size() > kMaxCookedCount)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Cooked mesh is missing an id or a render mesh");
	}

	std::error_code errorCode{};
	std::filesystem::create_directories(path.parent_path(), errorCode);
	if (errorCode)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Cooked mesh directory could not be created");
	}

	BinaryWriter writer{};
	WriteMesh(writer, sourceHash, mesh);

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Cooked mesh file could not be opened");
	}

	file.write(reinterpret_cast<const char*>(writer.Data().data()), static_cast<std::streamsize>(writer.Data().size()));
	if (!file)
	{
		return FailRuntime(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Cooked mesh file could not be written");
	}

	return MakeOk();
}
