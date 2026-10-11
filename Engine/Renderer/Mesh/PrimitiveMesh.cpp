#include "Engine/Renderer/Mesh/PrimitiveMesh.h"

#include "Engine/Renderer/Mesh/VertexTangents.h"

#include "Engine/RHI/Common/RHIInput.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cmath>
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
	GenerateVertexTangents(mesh.vertices, mesh.indices);
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

	GenerateVertexTangents(mesh.vertices, mesh.indices);
	return MakeOk(std::move(mesh));
}

Result<PrimitiveMeshData> GenerateSphereMesh(const SpherePrimitiveDesc& desc)
{
	if (desc.radius <= 0.0f || desc.stacks < 2 || desc.slices < 3)
	{
		return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"GenerateSphereMesh requires a positive radius, at least 2 stacks, and at least 3 slices");
	}

	PrimitiveMeshData mesh{};
	ResetBounds(mesh.bounds);

	const uint32_t stacks = desc.stacks;
	const uint32_t slices = desc.slices;
	const float radius = desc.radius;
	const uint32_t ringCount = stacks - 1;

	auto addVertex = [&mesh, radius](float x, float y, float z, float u, float v) {
		BasicVertex vertex{};
		vertex.position[0] = x;
		vertex.position[1] = y;
		vertex.position[2] = z;
		vertex.position[3] = 1.0f;
		vertex.uv[0] = u;
		vertex.uv[1] = v;
		vertex.normal[0] = x / radius;
		vertex.normal[1] = y / radius;
		vertex.normal[2] = z / radius;
		ExpandBounds(mesh.bounds, vertex.position);
		const uint32_t index = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back(vertex);
		return index;
	};

	auto addOrientedTriangle = [&mesh](uint32_t index0, uint32_t index1, uint32_t index2) {
		const BasicVertex& vertex0 = mesh.vertices[index0];
		const BasicVertex& vertex1 = mesh.vertices[index1];
		const BasicVertex& vertex2 = mesh.vertices[index2];
		const float edge1X = vertex1.position[0] - vertex0.position[0];
		const float edge1Y = vertex1.position[1] - vertex0.position[1];
		const float edge1Z = vertex1.position[2] - vertex0.position[2];
		const float edge2X = vertex2.position[0] - vertex0.position[0];
		const float edge2Y = vertex2.position[1] - vertex0.position[1];
		const float edge2Z = vertex2.position[2] - vertex0.position[2];
		const float crossX = (edge1Y * edge2Z) - (edge1Z * edge2Y);
		const float crossY = (edge1Z * edge2X) - (edge1X * edge2Z);
		const float crossZ = (edge1X * edge2Y) - (edge1Y * edge2X);
		const float alignment =
			(crossX * vertex0.normal[0]) + (crossY * vertex0.normal[1]) + (crossZ * vertex0.normal[2]);
		if (alignment < 0.0f)
		{
			std::swap(index1, index2);
		}

		mesh.indices.push_back(index0);
		mesh.indices.push_back(index1);
		mesh.indices.push_back(index2);
	};

	const auto ringVertex = [slices](uint32_t ring, uint32_t slice) {
		return 1u + (ring * slices) + slice;
	};

	addVertex(0.0f, radius, 0.0f, 0.5f, 0.0f);
	for (uint32_t stack = 1; stack < stacks; ++stack)
	{
		const float v = static_cast<float>(stack) / static_cast<float>(stacks);
		const float phi = v * DirectX::XM_PI;
		const float ringY = radius * std::cos(phi);
		const float ringRadius = radius * std::sin(phi);
		for (uint32_t slice = 0; slice < slices; ++slice)
		{
			const float u = static_cast<float>(slice) / static_cast<float>(slices);
			const float theta = u * DirectX::XM_2PI;
			addVertex(ringRadius * std::cos(theta), ringY, ringRadius * std::sin(theta), u, v);
		}
	}
	const uint32_t southPole = addVertex(0.0f, -radius, 0.0f, 0.5f, 1.0f);
	const uint32_t northPole = 0;

	for (uint32_t slice = 0; slice < slices; ++slice)
	{
		const uint32_t nextSlice = (slice + 1) % slices;
		addOrientedTriangle(northPole, ringVertex(0, slice), ringVertex(0, nextSlice));
	}

	for (uint32_t ring = 0; ring + 1 < ringCount; ++ring)
	{
		for (uint32_t slice = 0; slice < slices; ++slice)
		{
			const uint32_t nextSlice = (slice + 1) % slices;
			const uint32_t topLeft = ringVertex(ring, slice);
			const uint32_t topRight = ringVertex(ring, nextSlice);
			const uint32_t bottomLeft = ringVertex(ring + 1, slice);
			const uint32_t bottomRight = ringVertex(ring + 1, nextSlice);
			addOrientedTriangle(topLeft, topRight, bottomLeft);
			addOrientedTriangle(topRight, bottomRight, bottomLeft);
		}
	}

	const uint32_t southRing = ringCount - 1;
	for (uint32_t slice = 0; slice < slices; ++slice)
	{
		const uint32_t nextSlice = (slice + 1) % slices;
		addOrientedTriangle(southPole, ringVertex(southRing, nextSlice), ringVertex(southRing, slice));
	}

	GenerateVertexTangents(mesh.vertices, mesh.indices);
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
