#include "Engine/Renderer/Mesh/PrimitiveMesh.h"

#include "Engine/RHI/Common/RHIInput.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace
{
void ExpandBounds(MeshBounds& bounds, const float position[3])
{
	bounds.minX = (std::min)(bounds.minX, position[0]);
	bounds.minY = (std::min)(bounds.minY, position[1]);
	bounds.minZ = (std::min)(bounds.minZ, position[2]);
	bounds.maxX = (std::max)(bounds.maxX, position[0]);
	bounds.maxY = (std::max)(bounds.maxY, position[1]);
	bounds.maxZ = (std::max)(bounds.maxZ, position[2]);
}

void ResetBounds(MeshBounds& bounds)
{
	const float maxValue = (std::numeric_limits<float>::max)();
	bounds.minX = bounds.minY = bounds.minZ = maxValue;
	bounds.maxX = bounds.maxY = bounds.maxZ = std::numeric_limits<float>::lowest();
}

// u and v are half-extent vectors spanning the face. Triangles are clockwise as seen from the
// side the normal points to, matching D3D's default front-face convention.
void AddFace(
	PrimitiveMeshData& mesh,
	DirectX::XMFLOAT3 center,
	DirectX::XMFLOAT3 normal,
	DirectX::XMFLOAT3 u,
	DirectX::XMFLOAT3 v)
{
	using namespace DirectX;

	const XMVECTOR c = XMLoadFloat3(&center);
	const XMVECTOR n = XMLoadFloat3(&normal);
	XMVECTOR uAxis = XMLoadFloat3(&u);
	const XMVECTOR vAxis = XMLoadFloat3(&v);
	if (XMVectorGetX(XMVector3Dot(XMVector3Cross(vAxis, uAxis), n)) < 0.0f)
	{
		uAxis = XMVectorNegate(uAxis);
	}

	const std::array<XMVECTOR, 4> corners = {
		XMVectorSubtract(XMVectorSubtract(c, uAxis), vAxis),
		XMVectorAdd(XMVectorSubtract(c, uAxis), vAxis),
		XMVectorAdd(XMVectorAdd(c, uAxis), vAxis),
		XMVectorSubtract(XMVectorAdd(c, uAxis), vAxis),
	};
	constexpr std::array<std::array<float, 2>, 4> uvs = { {
		{ 0.0f, 1.0f },
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
	} };

	const uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
	for (std::size_t i = 0; i < corners.size(); ++i)
	{
		XMFLOAT3 position{};
		XMStoreFloat3(&position, corners[i]);

		BasicVertex vertex{};
		vertex.position[0] = position.x;
		vertex.position[1] = position.y;
		vertex.position[2] = position.z;
		vertex.position[3] = 1.0f;
		vertex.uv[0] = uvs[i][0];
		vertex.uv[1] = uvs[i][1];
		vertex.normal[0] = normal.x;
		vertex.normal[1] = normal.y;
		vertex.normal[2] = normal.z;
		ExpandBounds(mesh.bounds, vertex.position);
		mesh.vertices.push_back(vertex);
	}

	for (const uint32_t offset : { 0u, 1u, 2u, 0u, 2u, 3u })
	{
		mesh.indices.push_back(baseIndex + offset);
	}
}

void InvertFacing(PrimitiveMeshData& mesh)
{
	for (BasicVertex& vertex : mesh.vertices)
	{
		vertex.normal[0] = -vertex.normal[0];
		vertex.normal[1] = -vertex.normal[1];
		vertex.normal[2] = -vertex.normal[2];
	}

	for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
	{
		std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
	}
}
} // namespace

Result<PrimitiveMeshData> GeneratePlaneMesh(const PlanePrimitiveDesc& desc)
{
	if (desc.width <= 0.0f || desc.depth <= 0.0f)
	{
		return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"GeneratePlaneMesh requires positive width and depth");
	}

	PrimitiveMeshData mesh{};
	ResetBounds(mesh.bounds);
	mesh.vertices.reserve(4);
	mesh.indices.reserve(6);

	const float halfX = desc.width * 0.5f;
	const float halfZ = desc.depth * 0.5f;
	AddFace(mesh, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { halfX, 0.0f, 0.0f }, { 0.0f, 0.0f, halfZ });
	return MakeOk(std::move(mesh));
}

Result<PrimitiveMeshData> GenerateCubeMesh(const CubePrimitiveDesc& desc)
{
	if (desc.width <= 0.0f || desc.height <= 0.0f || desc.depth <= 0.0f)
	{
		return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"GenerateCubeMesh requires positive width, height, and depth");
	}

	PrimitiveMeshData mesh{};
	ResetBounds(mesh.bounds);
	mesh.vertices.reserve(24);
	mesh.indices.reserve(36);

	const float halfX = desc.width * 0.5f;
	const float halfY = desc.height * 0.5f;
	const float halfZ = desc.depth * 0.5f;

	AddFace(mesh, { 0.0f, halfY, 0.0f }, { 0.0f, 1.0f, 0.0f }, { halfX, 0.0f, 0.0f }, { 0.0f, 0.0f, halfZ });
	AddFace(mesh, { 0.0f, -halfY, 0.0f }, { 0.0f, -1.0f, 0.0f }, { halfX, 0.0f, 0.0f }, { 0.0f, 0.0f, halfZ });
	AddFace(mesh, { 0.0f, 0.0f, halfZ }, { 0.0f, 0.0f, 1.0f }, { halfX, 0.0f, 0.0f }, { 0.0f, halfY, 0.0f });
	AddFace(mesh, { 0.0f, 0.0f, -halfZ }, { 0.0f, 0.0f, -1.0f }, { halfX, 0.0f, 0.0f }, { 0.0f, halfY, 0.0f });
	AddFace(mesh, { halfX, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, halfZ }, { 0.0f, halfY, 0.0f });
	AddFace(mesh, { -halfX, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, halfZ }, { 0.0f, halfY, 0.0f });

	if (desc.inwardFacing)
	{
		InvertFacing(mesh);
	}

	return MakeOk(std::move(mesh));
}

MeshUploadDesc MakePrimitiveMeshUploadDesc(const PrimitiveMeshData& mesh, const char* debugName)
{
	MeshUploadDesc desc{};
	desc.vertices = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(mesh.vertices.data()),
		mesh.vertices.size() * sizeof(BasicVertex));
	desc.indices = std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(mesh.indices.data()),
		mesh.indices.size() * sizeof(uint32_t));
	desc.vertexStride = sizeof(BasicVertex);
	desc.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
	desc.indexCount = static_cast<uint32_t>(mesh.indices.size());
	desc.indexFormat = IndexFormat::R32_UINT;
	desc.layoutId = VertexLayoutId::Basic;
	desc.submeshes.push_back(SubmeshRange{ 0, desc.indexCount, 0 });
	desc.bounds = mesh.bounds;
	desc.DebugName = debugName;
	return desc;
}
