#include "Engine/Renderer/Mesh/VertexTangents.h"

#include <cmath>
#include <vector>

namespace
{
struct TangentAccum
{
	float tangent[3]{};
	float bitangent[3]{};
};

void AddScaled(float destination[3], const float source[3], float scale)
{
	destination[0] += source[0] * scale;
	destination[1] += source[1] * scale;
	destination[2] += source[2] * scale;
}

float Length(const float value[3])
{
	return std::sqrt((value[0] * value[0]) + (value[1] * value[1]) + (value[2] * value[2]));
}

void NormalizeOrFallback(float value[3], float fallbackX, float fallbackY, float fallbackZ)
{
	const float length = Length(value);
	if (length < 1.0e-8f)
	{
		value[0] = fallbackX;
		value[1] = fallbackY;
		value[2] = fallbackZ;
		return;
	}

	const float inverse = 1.0f / length;
	value[0] *= inverse;
	value[1] *= inverse;
	value[2] *= inverse;
}
} // namespace

void GenerateVertexTangents(std::vector<BasicVertex>& vertices, std::span<const uint32_t> indices)
{
	std::vector<TangentAccum> accumulated(vertices.size());
	for (size_t index = 0; index + 2 < indices.size(); index += 3)
	{
		const uint32_t index0 = indices[index];
		const uint32_t index1 = indices[index + 1];
		const uint32_t index2 = indices[index + 2];
		if (index0 >= vertices.size() || index1 >= vertices.size() || index2 >= vertices.size())
		{
			continue;
		}

		const BasicVertex& vertex0 = vertices[index0];
		const BasicVertex& vertex1 = vertices[index1];
		const BasicVertex& vertex2 = vertices[index2];
		const float edge1[3] = {
			vertex1.position[0] - vertex0.position[0],
			vertex1.position[1] - vertex0.position[1],
			vertex1.position[2] - vertex0.position[2],
		};
		const float edge2[3] = {
			vertex2.position[0] - vertex0.position[0],
			vertex2.position[1] - vertex0.position[1],
			vertex2.position[2] - vertex0.position[2],
		};
		const float deltaU1 = vertex1.uv[0] - vertex0.uv[0];
		const float deltaV1 = vertex1.uv[1] - vertex0.uv[1];
		const float deltaU2 = vertex2.uv[0] - vertex0.uv[0];
		const float deltaV2 = vertex2.uv[1] - vertex0.uv[1];
		const float determinant = (deltaU1 * deltaV2) - (deltaU2 * deltaV1);
		if (std::fabs(determinant) < 1.0e-8f)
		{
			continue;
		}

		const float inverseDeterminant = 1.0f / determinant;
		const float tangent[3] = {
			((edge1[0] * deltaV2) - (edge2[0] * deltaV1)) * inverseDeterminant,
			((edge1[1] * deltaV2) - (edge2[1] * deltaV1)) * inverseDeterminant,
			((edge1[2] * deltaV2) - (edge2[2] * deltaV1)) * inverseDeterminant,
		};
		const float bitangent[3] = {
			((edge2[0] * deltaU1) - (edge1[0] * deltaU2)) * inverseDeterminant,
			((edge2[1] * deltaU1) - (edge1[1] * deltaU2)) * inverseDeterminant,
			((edge2[2] * deltaU1) - (edge1[2] * deltaU2)) * inverseDeterminant,
		};

		for (const uint32_t vertexIndex : { index0, index1, index2 })
		{
			AddScaled(accumulated[vertexIndex].tangent, tangent, 1.0f);
			AddScaled(accumulated[vertexIndex].bitangent, bitangent, 1.0f);
		}
	}

	for (size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex)
	{
		BasicVertex& vertex = vertices[vertexIndex];
		float normal[3] = { vertex.normal[0], vertex.normal[1], vertex.normal[2] };
		NormalizeOrFallback(normal, 0.0f, 1.0f, 0.0f);

		float tangent[3] = {
			accumulated[vertexIndex].tangent[0],
			accumulated[vertexIndex].tangent[1],
			accumulated[vertexIndex].tangent[2],
		};
		const float normalDotTangent =
			(normal[0] * tangent[0]) + (normal[1] * tangent[1]) + (normal[2] * tangent[2]);
		tangent[0] -= normal[0] * normalDotTangent;
		tangent[1] -= normal[1] * normalDotTangent;
		tangent[2] -= normal[2] * normalDotTangent;
		if (Length(tangent) < 1.0e-8f)
		{
			const float axis[3] = { std::fabs(normal[0]) < 0.9f ? 1.0f : 0.0f, 0.0f, std::fabs(normal[0]) < 0.9f ? 0.0f : 1.0f };
			tangent[0] = (axis[1] * normal[2]) - (axis[2] * normal[1]);
			tangent[1] = (axis[2] * normal[0]) - (axis[0] * normal[2]);
			tangent[2] = (axis[0] * normal[1]) - (axis[1] * normal[0]);
		}
		NormalizeOrFallback(tangent, 1.0f, 0.0f, 0.0f);

		const float bitangent[3] = {
			accumulated[vertexIndex].bitangent[0],
			accumulated[vertexIndex].bitangent[1],
			accumulated[vertexIndex].bitangent[2],
		};
		const float crossNormalTangent[3] = {
			(normal[1] * tangent[2]) - (normal[2] * tangent[1]),
			(normal[2] * tangent[0]) - (normal[0] * tangent[2]),
			(normal[0] * tangent[1]) - (normal[1] * tangent[0]),
		};
		const float handedness =
			(crossNormalTangent[0] * bitangent[0]) +
			(crossNormalTangent[1] * bitangent[1]) +
			(crossNormalTangent[2] * bitangent[2]);

		vertex.tangent[0] = tangent[0];
		vertex.tangent[1] = tangent[1];
		vertex.tangent[2] = tangent[2];
		vertex.tangent[3] = handedness < 0.0f ? -1.0f : 1.0f;
	}
}
