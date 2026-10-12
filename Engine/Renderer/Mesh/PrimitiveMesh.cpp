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

void AddOutwardTriangle(PrimitiveMeshData& mesh, uint32_t index0, uint32_t index1, uint32_t index2)
{
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
}

struct RingRange
{
	uint32_t start = 0;
	uint32_t count = 0;
};

void ConnectRings(PrimitiveMeshData& mesh, RingRange previous, RingRange next)
{
	if (previous.count == 1)
	{
		for (uint32_t slice = 0; slice < next.count; ++slice)
		{
			const uint32_t nextSlice = (slice + 1) % next.count;
			AddOutwardTriangle(mesh, previous.start, next.start + slice, next.start + nextSlice);
		}
		return;
	}

	if (next.count == 1)
	{
		for (uint32_t slice = 0; slice < previous.count; ++slice)
		{
			const uint32_t nextSlice = (slice + 1) % previous.count;
			AddOutwardTriangle(mesh, next.start, previous.start + nextSlice, previous.start + slice);
		}
		return;
	}

	for (uint32_t slice = 0; slice < previous.count; ++slice)
	{
		const uint32_t nextSlice = (slice + 1) % previous.count;
		const uint32_t topLeft = previous.start + slice;
		const uint32_t topRight = previous.start + nextSlice;
		const uint32_t bottomLeft = next.start + slice;
		const uint32_t bottomRight = next.start + nextSlice;
		AddOutwardTriangle(mesh, topLeft, topRight, bottomLeft);
		AddOutwardTriangle(mesh, topRight, bottomRight, bottomLeft);
	}
}

bool AppendFlatTriangle(
	PrimitiveMeshData& mesh,
	DirectX::XMFLOAT3 p0,
	DirectX::XMFLOAT3 p1,
	DirectX::XMFLOAT3 p2)
{
	const float edge1X = p1.x - p0.x;
	const float edge1Y = p1.y - p0.y;
	const float edge1Z = p1.z - p0.z;
	const float edge2X = p2.x - p0.x;
	const float edge2Y = p2.y - p0.y;
	const float edge2Z = p2.z - p0.z;
	float normalX = (edge1Y * edge2Z) - (edge1Z * edge2Y);
	float normalY = (edge1Z * edge2X) - (edge1X * edge2Z);
	float normalZ = (edge1X * edge2Y) - (edge1Y * edge2X);
	const float normalLength = std::sqrt((normalX * normalX) + (normalY * normalY) + (normalZ * normalZ));
	if (normalLength < 1.0e-8f)
	{
		return false;
	}

	const float inverseNormal = 1.0f / normalLength;
	normalX *= inverseNormal;
	normalY *= inverseNormal;
	normalZ *= inverseNormal;

	const float edgeLength = std::sqrt((edge1X * edge1X) + (edge1Y * edge1Y) + (edge1Z * edge1Z));
	const float inverseEdge = 1.0f / edgeLength;
	const float tangentX = edge1X * inverseEdge;
	const float tangentY = edge1Y * inverseEdge;
	const float tangentZ = edge1Z * inverseEdge;
	const float bitangentX = (normalY * tangentZ) - (normalZ * tangentY);
	const float bitangentY = (normalZ * tangentX) - (normalX * tangentZ);
	const float bitangentZ = (normalX * tangentY) - (normalY * tangentX);

	const std::array<DirectX::XMFLOAT3, 3> positions = { p0, p1, p2 };
	const uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
	for (const DirectX::XMFLOAT3& position : positions)
	{
		const float deltaX = position.x - p0.x;
		const float deltaY = position.y - p0.y;
		const float deltaZ = position.z - p0.z;

		BasicVertex vertex{};
		vertex.position[0] = position.x;
		vertex.position[1] = position.y;
		vertex.position[2] = position.z;
		vertex.position[3] = 1.0f;
		vertex.uv[0] = (deltaX * tangentX) + (deltaY * tangentY) + (deltaZ * tangentZ);
		vertex.uv[1] = (deltaX * bitangentX) + (deltaY * bitangentY) + (deltaZ * bitangentZ);
		vertex.normal[0] = normalX;
		vertex.normal[1] = normalY;
		vertex.normal[2] = normalZ;
		ExpandBounds(mesh.bounds, vertex.position);
		mesh.vertices.push_back(vertex);
	}

	mesh.indices.push_back(baseIndex);
	mesh.indices.push_back(baseIndex + 1);
	mesh.indices.push_back(baseIndex + 2);
	return true;
}

struct HullPoint
{
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct HullFace
{
	int a = 0;
	int b = 0;
	int c = 0;
};

[[nodiscard]] double Triple(const HullPoint& a, const HullPoint& b, const HullPoint& c, const HullPoint& point)
{
	const double abx = b.x - a.x;
	const double aby = b.y - a.y;
	const double abz = b.z - a.z;
	const double acx = c.x - a.x;
	const double acy = c.y - a.y;
	const double acz = c.z - a.z;
	const double apx = point.x - a.x;
	const double apy = point.y - a.y;
	const double apz = point.z - a.z;
	const double normalX = (aby * acz) - (abz * acy);
	const double normalY = (abz * acx) - (abx * acz);
	const double normalZ = (abx * acy) - (aby * acx);
	return (normalX * apx) + (normalY * apy) + (normalZ * apz);
}

[[nodiscard]] bool InsideOrOnTriangle(
	const HullPoint& a,
	const HullPoint& b,
	const HullPoint& c,
	const HullPoint& point,
	double epsilon)
{
	auto sameSide = [epsilon](
						const HullPoint& from,
						const HullPoint& to,
						const HullPoint& opposite,
						const HullPoint& sample) {
		const double edgeX = to.x - from.x;
		const double edgeY = to.y - from.y;
		const double edgeZ = to.z - from.z;
		const double sampleX = sample.x - from.x;
		const double sampleY = sample.y - from.y;
		const double sampleZ = sample.z - from.z;
		const double farX = opposite.x - from.x;
		const double farY = opposite.y - from.y;
		const double farZ = opposite.z - from.z;
		const double sampleCrossX = (edgeY * sampleZ) - (edgeZ * sampleY);
		const double sampleCrossY = (edgeZ * sampleX) - (edgeX * sampleZ);
		const double sampleCrossZ = (edgeX * sampleY) - (edgeY * sampleX);
		const double farCrossX = (edgeY * farZ) - (edgeZ * farY);
		const double farCrossY = (edgeZ * farX) - (edgeX * farZ);
		const double farCrossZ = (edgeX * farY) - (edgeY * farX);
		const double alignment =
			(sampleCrossX * farCrossX) + (sampleCrossY * farCrossY) + (sampleCrossZ * farCrossZ);
		return alignment >= -epsilon;
	};

	return sameSide(a, b, c, point) && sameSide(b, c, a, point) && sameSide(c, a, b, point);
}

[[nodiscard]] bool BuildConvexHullFaces(
	const std::vector<HullPoint>& points,
	double volumeEpsilon,
	double planeEpsilon,
	std::vector<HullFace>& faces)
{
	const int count = static_cast<int>(points.size());
	int i0 = 0;
	for (int index = 1; index < count; ++index)
	{
		const HullPoint& candidate = points[static_cast<size_t>(index)];
		const HullPoint& current = points[static_cast<size_t>(i0)];
		if (candidate.x < current.x ||
			(candidate.x == current.x && candidate.y < current.y) ||
			(candidate.x == current.x && candidate.y == current.y && candidate.z < current.z))
		{
			i0 = index;
		}
	}

	int i1 = -1;
	double bestDistance = 0.0;
	for (int index = 0; index < count; ++index)
	{
		if (index == i0)
		{
			continue;
		}

		const double dx = points[static_cast<size_t>(index)].x - points[static_cast<size_t>(i0)].x;
		const double dy = points[static_cast<size_t>(index)].y - points[static_cast<size_t>(i0)].y;
		const double dz = points[static_cast<size_t>(index)].z - points[static_cast<size_t>(i0)].z;
		const double distance = (dx * dx) + (dy * dy) + (dz * dz);
		if (distance > bestDistance)
		{
			bestDistance = distance;
			i1 = index;
		}
	}
	if (i1 < 0)
	{
		return false;
	}

	int i2 = -1;
	double bestArea = 0.0;
	for (int index = 0; index < count; ++index)
	{
		if (index == i0 || index == i1)
		{
			continue;
		}

		const double abx = points[static_cast<size_t>(i1)].x - points[static_cast<size_t>(i0)].x;
		const double aby = points[static_cast<size_t>(i1)].y - points[static_cast<size_t>(i0)].y;
		const double abz = points[static_cast<size_t>(i1)].z - points[static_cast<size_t>(i0)].z;
		const double acx = points[static_cast<size_t>(index)].x - points[static_cast<size_t>(i0)].x;
		const double acy = points[static_cast<size_t>(index)].y - points[static_cast<size_t>(i0)].y;
		const double acz = points[static_cast<size_t>(index)].z - points[static_cast<size_t>(i0)].z;
		const double crossX = (aby * acz) - (abz * acy);
		const double crossY = (abz * acx) - (abx * acz);
		const double crossZ = (abx * acy) - (aby * acx);
		const double area = (crossX * crossX) + (crossY * crossY) + (crossZ * crossZ);
		if (area > bestArea)
		{
			bestArea = area;
			i2 = index;
		}
	}
	if (i2 < 0 || bestArea <= volumeEpsilon)
	{
		return false;
	}

	int i3 = -1;
	double bestVolume = 0.0;
	for (int index = 0; index < count; ++index)
	{
		if (index == i0 || index == i1 || index == i2)
		{
			continue;
		}

		const double volume = std::fabs(Triple(
			points[static_cast<size_t>(i0)],
			points[static_cast<size_t>(i1)],
			points[static_cast<size_t>(i2)],
			points[static_cast<size_t>(index)]));
		if (volume > bestVolume)
		{
			bestVolume = volume;
			i3 = index;
		}
	}
	if (i3 < 0 || bestVolume <= volumeEpsilon)
	{
		return false;
	}

	if (Triple(
			points[static_cast<size_t>(i0)],
			points[static_cast<size_t>(i1)],
			points[static_cast<size_t>(i2)],
			points[static_cast<size_t>(i3)]) > 0.0)
	{
		std::swap(i1, i2);
	}

	const HullPoint inside{
		(points[static_cast<size_t>(i0)].x + points[static_cast<size_t>(i1)].x +
			points[static_cast<size_t>(i2)].x + points[static_cast<size_t>(i3)].x) *
			0.25,
		(points[static_cast<size_t>(i0)].y + points[static_cast<size_t>(i1)].y +
			points[static_cast<size_t>(i2)].y + points[static_cast<size_t>(i3)].y) *
			0.25,
		(points[static_cast<size_t>(i0)].z + points[static_cast<size_t>(i1)].z +
			points[static_cast<size_t>(i2)].z + points[static_cast<size_t>(i3)].z) *
			0.25,
	};

	faces = {
		{ i0, i1, i2 },
		{ i1, i0, i3 },
		{ i2, i1, i3 },
		{ i0, i2, i3 },
	};

	for (int index = 0; index < count; ++index)
	{
		if (index == i0 || index == i1 || index == i2 || index == i3)
		{
			continue;
		}

		std::vector<HullFace> visible;
		visible.reserve(faces.size());
		std::vector<HullFace> kept;
		kept.reserve(faces.size());
		for (const HullFace& face : faces)
		{
			const HullPoint& a = points[static_cast<size_t>(face.a)];
			const HullPoint& b = points[static_cast<size_t>(face.b)];
			const HullPoint& c = points[static_cast<size_t>(face.c)];
			const HullPoint& sample = points[static_cast<size_t>(index)];
			const double side = Triple(a, b, c, sample);
			const bool outside = side > volumeEpsilon ||
				(std::fabs(side) <= volumeEpsilon && !InsideOrOnTriangle(a, b, c, sample, planeEpsilon));
			if (outside)
			{
				visible.push_back(face);
			}
			else
			{
				kept.push_back(face);
			}
		}
		if (visible.empty())
		{
			continue;
		}

		struct DirectedEdge
		{
			int from = 0;
			int to = 0;
		};
		std::vector<DirectedEdge> edges;
		edges.reserve(visible.size() * 3);
		for (const HullFace& face : visible)
		{
			edges.push_back({ face.a, face.b });
			edges.push_back({ face.b, face.c });
			edges.push_back({ face.c, face.a });
		}

		std::vector<HullFace> next = std::move(kept);
		for (const DirectedEdge& edge : edges)
		{
			bool shared = false;
			for (const DirectedEdge& other : edges)
			{
				if (other.from == edge.to && other.to == edge.from)
				{
					shared = true;
					break;
				}
			}
			if (shared)
			{
				continue;
			}

			HullFace face{ edge.to, edge.from, index };
			const double side = Triple(
				points[static_cast<size_t>(face.a)],
				points[static_cast<size_t>(face.b)],
				points[static_cast<size_t>(face.c)],
				inside);
			if (side > volumeEpsilon)
			{
				std::swap(face.a, face.b);
			}
			else if (side >= -volumeEpsilon)
			{
				return false;
			}
			next.push_back(face);
		}

		faces = std::move(next);
	}

	return !faces.empty();
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
		AddOutwardTriangle(mesh, northPole, ringVertex(0, slice), ringVertex(0, nextSlice));
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
			AddOutwardTriangle(mesh, topLeft, topRight, bottomLeft);
			AddOutwardTriangle(mesh, topRight, bottomRight, bottomLeft);
		}
	}

	const uint32_t southRing = ringCount - 1;
	for (uint32_t slice = 0; slice < slices; ++slice)
	{
		const uint32_t nextSlice = (slice + 1) % slices;
		AddOutwardTriangle(mesh, southPole, ringVertex(southRing, nextSlice), ringVertex(southRing, slice));
	}

	GenerateVertexTangents(mesh.vertices, mesh.indices);
	return MakeOk(std::move(mesh));
}

Result<PrimitiveMeshData> GenerateCapsuleMesh(const CapsulePrimitiveDesc& desc)
{
	if (desc.radius <= 0.0f || desc.halfHeight <= 0.0f || desc.slices < 3 || desc.hemisphereStacks < 1)
	{
		return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"GenerateCapsuleMesh requires a positive radius and half height, at least 1 hemisphere stack, and at least 3 slices");
	}

	PrimitiveMeshData mesh{};
	ResetBounds(mesh.bounds);

	const uint32_t slices = desc.slices;
	const uint32_t stacks = desc.hemisphereStacks;
	const float radius = desc.radius;
	const float halfHeight = desc.halfHeight;
	const float hemisphereArc = radius * DirectX::XM_PIDIV2;
	const float totalArc = (hemisphereArc * 2.0f) + (halfHeight * 2.0f);

	auto addVertex = [&](float x, float y, float z, float normalX, float normalY, float normalZ, float u, float v) {
		BasicVertex vertex{};
		vertex.position[0] = x;
		vertex.position[1] = y;
		vertex.position[2] = z;
		vertex.position[3] = 1.0f;
		vertex.uv[0] = u;
		vertex.uv[1] = v;
		vertex.normal[0] = normalX;
		vertex.normal[1] = normalY;
		vertex.normal[2] = normalZ;
		ExpandBounds(mesh.bounds, vertex.position);
		const uint32_t index = static_cast<uint32_t>(mesh.vertices.size());
		mesh.vertices.push_back(vertex);
		return index;
	};

	auto addRing = [&](float centerY, float phi, float v) {
		const float ringY = centerY + (radius * std::cos(phi));
		const float ringRadius = radius * std::sin(phi);
		const uint32_t start = static_cast<uint32_t>(mesh.vertices.size());
		for (uint32_t slice = 0; slice < slices; ++slice)
		{
			const float u = static_cast<float>(slice) / static_cast<float>(slices);
			const float theta = u * DirectX::XM_2PI;
			const float normalX = std::cos(theta);
			const float normalZ = std::sin(theta);
			const float x = ringRadius * normalX;
			const float z = ringRadius * normalZ;
			const float normalY = std::cos(phi);
			addVertex(x, ringY, z, normalX * std::sin(phi), normalY, normalZ * std::sin(phi), u, v);
		}
		return RingRange{ start, slices };
	};

	std::vector<RingRange> rings;
	rings.reserve(static_cast<size_t>(stacks) * 2 + 2);

	const uint32_t northPole = addVertex(0.0f, halfHeight + radius, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f);
	rings.push_back(RingRange{ northPole, 1 });

	for (uint32_t stack = 1; stack <= stacks; ++stack)
	{
		const float phi = (static_cast<float>(stack) / static_cast<float>(stacks)) * DirectX::XM_PIDIV2;
		const float v = (phi * radius) / totalArc;
		rings.push_back(addRing(halfHeight, phi, v));
	}

	const float bottomEquatorV = (hemisphereArc + (halfHeight * 2.0f)) / totalArc;
	rings.push_back(addRing(-halfHeight, DirectX::XM_PIDIV2, bottomEquatorV));

	for (uint32_t stack = 1; stack < stacks; ++stack)
	{
		const float phi = DirectX::XM_PIDIV2 +
			((static_cast<float>(stack) / static_cast<float>(stacks)) * DirectX::XM_PIDIV2);
		const float v = (hemisphereArc + (halfHeight * 2.0f) + ((phi - DirectX::XM_PIDIV2) * radius)) / totalArc;
		rings.push_back(addRing(-halfHeight, phi, v));
	}

	const uint32_t southPole = addVertex(0.0f, -halfHeight - radius, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 1.0f);
	rings.push_back(RingRange{ southPole, 1 });

	for (size_t ring = 0; ring + 1 < rings.size(); ++ring)
	{
		ConnectRings(mesh, rings[ring], rings[ring + 1]);
	}

	GenerateVertexTangents(mesh.vertices, mesh.indices);
	return MakeOk(std::move(mesh));
}

Result<PrimitiveMeshData> GenerateConvexHullMesh(const ConvexHullPrimitiveDesc& desc)
{
	if (desc.points.size() < 4)
	{
		return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"GenerateConvexHullMesh requires at least four points");
	}

	std::vector<HullPoint> points;
	points.reserve(desc.points.size());
	double scale = 0.0;
	for (const DirectX::XMFLOAT3& point : desc.points)
	{
		if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
		{
			return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
				"GenerateConvexHullMesh requires finite points");
		}

		bool duplicate = false;
		for (const HullPoint& existing : points)
		{
			const double dx = existing.x - point.x;
			const double dy = existing.y - point.y;
			const double dz = existing.z - point.z;
			if ((dx * dx) + (dy * dy) + (dz * dz) <= 1.0e-12)
			{
				duplicate = true;
				break;
			}
		}
		if (duplicate)
		{
			continue;
		}

		points.push_back({ point.x, point.y, point.z });
		scale = (std::max)(scale, std::fabs(static_cast<double>(point.x)));
		scale = (std::max)(scale, std::fabs(static_cast<double>(point.y)));
		scale = (std::max)(scale, std::fabs(static_cast<double>(point.z)));
	}

	if (points.size() < 4)
	{
		return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"GenerateConvexHullMesh requires at least four distinct points");
	}

	const double extent = (std::max)(scale, 1.0);
	const double volumeEpsilon = 1.0e-8 * extent * extent * extent;
	const double planeEpsilon = 1.0e-8 * extent * extent * extent * extent;
	std::vector<HullFace> faces;
	if (!BuildConvexHullFaces(points, volumeEpsilon, planeEpsilon, faces))
	{
		return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"GenerateConvexHullMesh requires a non-degenerate volume");
	}

	PrimitiveMeshData mesh{};
	ResetBounds(mesh.bounds);
	for (const HullFace& face : faces)
	{
		const HullPoint& a = points[static_cast<size_t>(face.a)];
		const HullPoint& b = points[static_cast<size_t>(face.b)];
		const HullPoint& c = points[static_cast<size_t>(face.c)];
		AppendFlatTriangle(
			mesh,
			{ static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z) },
			{ static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.z) },
			{ static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.z) });
	}

	if (mesh.indices.empty())
	{
		return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"GenerateConvexHullMesh produced no triangles");
	}

	GenerateVertexTangents(mesh.vertices, mesh.indices);
	return MakeOk(std::move(mesh));
}

Result<PrimitiveMeshData> GenerateTriangleMesh(const TriangleMeshPrimitiveDesc& desc)
{
	if (desc.points.empty() || desc.indices.size() < 3 || (desc.indices.size() % 3) != 0)
	{
		return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"GenerateTriangleMesh requires points and a multiple of three indices");
	}

	for (const DirectX::XMFLOAT3& point : desc.points)
	{
		if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
		{
			return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
				"GenerateTriangleMesh requires finite points");
		}
	}

	PrimitiveMeshData mesh{};
	ResetBounds(mesh.bounds);
	for (size_t index = 0; index + 2 < desc.indices.size(); index += 3)
	{
		const uint32_t i0 = desc.indices[index];
		const uint32_t i1 = desc.indices[index + 1];
		const uint32_t i2 = desc.indices[index + 2];
		if (i0 >= desc.points.size() || i1 >= desc.points.size() || i2 >= desc.points.size())
		{
			return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
				"GenerateTriangleMesh index is out of range");
		}

		AppendFlatTriangle(mesh, desc.points[i0], desc.points[i1], desc.points[i2]);
	}

	if (mesh.indices.empty())
	{
		return FailRuntime<PrimitiveMeshData>(LogCategory::Renderer, ErrorCode::InvalidArgument,
			"GenerateTriangleMesh requires at least one non-degenerate triangle");
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
