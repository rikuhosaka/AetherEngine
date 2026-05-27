#pragma once


enum class InputLayoutType
{
	Basic,
	PositionTex,
	Skinned
};

enum class PrimitiveTopology
{
	TriangleList,
	TriangleStrip,
	LineList,
	PointList
};

enum class IndexFormat
{
	R16_UINT,
	R32_UINT
};